//
//  Copyright � 2017 VXG Inc. All rights reserved.
//  Contact: https://www.videoexpertsgroup.com/contact-vxg/
//  This file is part of the demonstration of the VXG Cloud Platform.
//
//  Commercial License Usage
//  Licensees holding valid commercial VXG licenses may use this file in
//  accordance with the commercial license agreement provided with the
//  Software or, alternatively, in accordance with the terms contained in
//  a written agreement between you and VXG Inc. For further information
//  use the contact form at https://www.videoexpertsgroup.com/contact-vxg/
//

#ifndef __CLOUDSHARECONNECTION_H__
#define __CLOUDSHARECONNECTION_H__

#include <cloud/CloudAPI.h>
#include <cloud/CloudReturnCodes.h>
#include <cloud/Uri.h>
#include <utils/utils.h>

class CloudShareConnection {
    vxg::logger::logger_ptr logger {vxg::logger::instance("cloud-connection")};
    CloudAPI mCloudAPI;
    bool mOpened;
    long long m_ServerTimeDiff;

public:
    CloudShareConnection() {
        mOpened = false;
        m_ServerTimeDiff = 0;
    }
    virtual ~CloudShareConnection() {}

    CloudAPI& _getCloudAPI() { return mCloudAPI; }

    int openSync(std::string& share_token) {
        std::string s = "http://cam.skyvr.videoexpertsgroup.com:8888";
        return openSync(share_token, s);
    }

    int openSync(std::string& share_token, std::string& baseurl) {
        std::string mProtocol = "http";
        std::string def_address = "cam.skyvr.videoexpertsgroup.com";
        std::string mHost = def_address;
        int mPort = 8888;
        std::string mPrefixPath = "";

        if (baseurl.length() > 1 && baseurl.at(baseurl.length() - 1) == '/') {
            baseurl = baseurl.substr(0, baseurl.length() - 1);
        }

        Uri uri = Uri::Parse(baseurl);
        mProtocol = uri.Protocol;
        mHost = uri.Host;
        mPort = atoi(uri.Port.c_str());
        if (mPort == 0)
            mPort = 8888;
        mPrefixPath = uri.Path;

        logger->info("Protocol: {}", mProtocol.c_str());
        logger->info("Host: {}", mHost.c_str());
        logger->info("Port: {}", mPort);
        logger->info("PrefixPath: {}", mPrefixPath.c_str());

        mHost = mHost + ":" + std::to_string(mPort);
        mCloudAPI.setHost(mHost);
        mCloudAPI.setProtocol(mProtocol);
        mCloudAPI.setPrefixPath(mPrefixPath);
        mCloudAPI.setShareToken(share_token);
        if (string::npos == mHost.find(def_address)) {
            mCloudAPI.setCMAddress(mHost);
        }
        mOpened = true;

        return RET_OK;
    }
};

#endif  //__CLOUDSHARECONNECTION_H__