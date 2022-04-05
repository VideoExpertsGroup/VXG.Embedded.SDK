#include <tests/test_Transport.h>

#include <net/websockets.h>

using namespace ::testing;
using namespace std;
using namespace transport::libwebsockets;

class TestTransport : public Test {
protected:
    TestTransport() {}

    virtual void SetUp() {}

    virtual void TearDown() {}

public:
};

class WSTest : public Test, public websocket {
protected:
    WSTest() {}

    virtual void SetUp() { start(); }
    virtual void TearDown() { stop(); }

public:
    bool __connect_sync(std::string url) {
        this->set_connected_cb([=]() {
            {
                std::lock_guard<std::mutex> lock(lock_);
                connected_ = true;
            }

            std::cout << "Connected cb" << std::endl;
            cond_conn_.notify_one();
        });

        this->set_error_cb([=](void* a, std::string err) {
            {
                std::lock_guard<std::mutex> lock(lock_);
                connect_error_ = true;
            }
            std::cout << "Error cb" << std::endl;
            cond_conn_.notify_one();
        });

        this->set_disconnected_cb([=]() {
            {
                std::lock_guard<std::mutex> lock(lock_);
                disconnected_ = true;
            }

            std::cout << "Disconnected cb" << std::endl;
            cond_conn_.notify_one();
        });

        {
            std::unique_lock<std::mutex> lock(lock_);
            connected_ = false;
            connect_error_ = false;
            disconnected_ = false;

            if (!this->connect(url))
                return false;

            // cond_conn_.wait_for(lock,
            //                          std::chrono::seconds(connect_timeout_),
            //                          [this]() { return connected_; });

            cond_conn_.wait(lock, [this]() {
                return connected_ || connect_error_ || disconnected_;
            });
            std::cout << (connected_ ? "Connected notified" : "Error notified")
                      << std::endl;
        }

        return connected_;
    }

    bool __disconnect_sync() {
        this->set_disconnected_cb([=]() {
            {
                std::lock_guard<std::mutex> lock(lock_);
                disconnected_ = true;
            }

            std::cout << "Disconnected cb" << std::endl;
            cond_conn_.notify_one();
        });

        {
            std::unique_lock<std::mutex> lock(lock_);
            disconnected_ = false;

            this->disconnect();

            // cond_conn_.wait_for(lock, std::chrono::seconds(connect_timeout_),
            //                     [this]() { return disconnected_; });
            cond_conn_.wait(lock, [this]() { return disconnected_; });
            std::cout << (disconnected_ ? "Disconnected notified"
                                        : "Disconnection timedout")
                      << std::endl;
        }

        return disconnected_;
    }

    std::mutex lock_;
    std::condition_variable cond_;
    std::condition_variable cond_conn_;
    std::chrono::seconds connect_timeout_ {std::chrono::seconds(25)};
    bool connected_ {false};
    bool connect_error_ {false};
    bool disconnected_ {false};
    bool done1_ {false};
    bool done2_ {false};
    bool timer1_ {false};
    bool timer2_ {false};
};

TEST_F(WSTest, ConnectStress) {
    int _n_reconnects = 1;
    int i = 0;

    for (; i < _n_reconnects; i++) {
        if (!__connect_sync(
                "wss://cm-27.inst.auth2.cloud-svcp.com:8883/ctl/NEW/"
                "share.eyJzaSI6IDIwMzk3MX0.605c87e9tbc1fab00.LCSHEKQK2B7B-"
                "ptfhGv2yKRu1yU/"))
            break;

        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        if (!__disconnect_sync())
            break;
    }

    EXPECT_EQ(i, _n_reconnects);
}

struct timer_test_struct {
    transport::timed_cb_ptr t_;
    bool done_;
    std::mutex lock_;
    std::condition_variable cond_;
    int timeout_ms;
    std::chrono::time_point<std::chrono::steady_clock> start;
    std::chrono::time_point<std::chrono::steady_clock> stop;
};

TEST_F(WSTest, WSScheduleTimedCB) {
    std::vector<std::shared_ptr<timer_test_struct>> _timers;
    size_t _n_timers = 10000;
    size_t i = 0;

    for (i = 0; i < _n_timers; i++) {
        auto test_struct = std::make_shared<timer_test_struct>();
        auto w = std::weak_ptr<timer_test_struct>(test_struct);

        test_struct->done_ = false;
        test_struct->timeout_ms = std::rand() % 10000;
        test_struct->start = std::chrono::steady_clock::now();

        std::cout << "Adding timer " << i << " timeout "
                  << test_struct->timeout_ms << " ms" << std::endl;

        test_struct->t_ = this->schedule_timed_cb(
            std::bind(
                [=](std::weak_ptr<timer_test_struct> w) {
                    {
                        if (auto t = w.lock()) {
                            {
                                std::lock_guard<std::mutex> lock(t->lock_);

                                t->done_ = true;
                                t->stop = std::chrono::steady_clock::now();
                                std::cout
                                    << "Timer " << i << " triggered after "
                                    << std::chrono::duration_cast<
                                           std::chrono::milliseconds>(t->stop -
                                                                      t->start)
                                           .count()
                                    << "ms" << std::endl;
                            }
                            std::cout << "Notify " << i << std::endl;
                            t->cond_.notify_one();
                        }
                    }
                },
                w),
            test_struct->timeout_ms);

        _timers.push_back(test_struct);
    }

    for (i = 0; i < _n_timers; i++) {
        if (std::rand() % 2) {
            std::cout << "Canceling timer " << i << std::endl;
            _timers[i]->done_ = true;
            this->cancel_timed_cb(_timers[i]->t_);
        }
    }

    for (i = 0; i < _n_timers; i++) {
        std::unique_lock<std::mutex> lock(_timers[i]->lock_);
        auto w = std::weak_ptr<timer_test_struct>(_timers[i]);

        std::cout << "Waiting for timer " << i << " done. Timeout "
                  << _timers[i]->timeout_ms << std::endl;

        if (!_timers[i]->done_)
            std::cout << "Cond WAIT " << i << std::endl;

        _timers[i]->cond_.wait(lock, [w]() {
            if (auto p = w.lock())
                return p->done_;
            else
                return true;
        });
        std::cout << "Timer " << i << " done." << std::endl;

        EXPECT_TRUE(_timers[i]->done_);
    }

    _timers.clear();
}
