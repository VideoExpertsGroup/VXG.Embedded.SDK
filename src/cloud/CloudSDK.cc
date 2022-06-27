#include "config.h"

#include "CloudSDK.h"

#ifndef VXGCLOUDAGENT_LIB_VERSION
#error "VXGCLOUDAGENT_LIB_VERSION is UNKNOWN, please specify it with define"
#endif

std::string CloudSDK::Version() {
    return VXGCLOUDAGENT_LIB_VERSION;
}
