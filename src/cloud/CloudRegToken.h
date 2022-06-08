
#ifndef __CLOUDREGTOKEN_H__
#define __CLOUDREGTOKEN_H__

#include <utils/utils.h>
#include <nlohmann/json.hpp>

using namespace std;
using json = nlohmann::json;

class CloudRegToken {
    string mToken;
    string mExpire;
    string mType;
    string mStatus;
    string mRtmpPublish;
    long long mCmngrID;

public:
    CloudRegToken() { mCmngrID = 0; }

    // FIXME: do we need separate class in this form?
    CloudRegToken(string json_data) {
        json jsonToken = json::parse(json_data);

        if (jsonToken.contains("token"))
            mToken = jsonToken["token"];
        if (jsonToken.contains("expire"))
            mExpire = jsonToken["expire"];
        if (jsonToken.contains("cmngr_id"))
            mCmngrID = jsonToken["cmngr_id"];
        if (jsonToken.contains("status"))
            mStatus = jsonToken["status"];
        if (jsonToken.contains("rtmp"))
            mRtmpPublish = jsonToken["rtmp"];
    }

    ~CloudRegToken() {}

    CloudRegToken& operator=(const CloudRegToken& src) {
        mToken = src.mToken;
        mExpire = src.mExpire;
        mType = src.mType;
        mStatus = src.mStatus;
        mRtmpPublish = src.mRtmpPublish;
        mCmngrID = src.mCmngrID;

        return *this;
    }

    bool isEmpty() { return (mToken.empty()); }

    string& getToken() { return mToken; }

    string& getExpire() { return mExpire; }

    string& getType() { return mType; }

    string& getStatus() { return mStatus; }
    string& getRtmpPublish() { return mRtmpPublish; }

    long long getCmngrID() { return mCmngrID; }
};

#endif  //__CLOUDREGTOKEN_H__