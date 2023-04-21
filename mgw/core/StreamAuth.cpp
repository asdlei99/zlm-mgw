#include "StreamAuth.h"

#include <openssl/md5.h>
#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#include <iostream>
#include <sstream>

using namespace std;

namespace mediakit {

std::string StreamAuth::md5Sum(const std::string &data) {

    cout << "md5 data:" << data << endl;
    string ret;
    MD5_CTX c;
    unsigned char md[MD5_DIGEST_LENGTH] = {};
    unsigned char res[MD5_DIGEST_LENGTH*2] = {};
    char *buf = (char*)res;
    if (1 != MD5_Init(&c))
        goto err;
    if (1 != MD5_Update(&c, data.c_str(), data.size()))
        goto err;
    if (1 != MD5_Final(md, &c))
        goto err;

    for (int i = 0; i < MD5_DIGEST_LENGTH; i++)
        buf += sprintf(buf, "%02x", md[i]);

    ret.append((const char *)res);

    cout << "md5 result:" << ret << endl;

err:
    return ret;
}

//rtmp://DomainName/AppName/StreamName?auth_key=method-timestamp-rand-uid-md5hash
//rtmp://192.168.0.15:1935/live/HDMN2DEV_C0?auth_key=push-1669813114-0-0-602c8645457f82b72fce392da3814038
std::string StreamAuth::getRtmpAddr(const std::string &host,
            const std::string &dev, const std::string &method,
            int time, uint32_t chn, uint16_t port) {

    ostringstream oss;
    oss << "rtmp://" << host << ":" << port;
    oss << "/live/" << dev << "_C" << chn;
    oss << "?auth_key=" << method << "-" << time << "-0-0-";

    //md5sum(uri+method-time+rand+uid+accessToken)
    string ret;
    ret = oss.str(); ret.append(md5Sum(string(oss.str()) + _key));

    //如果是知名端口号1935，去掉它
    if (port == 1935)
        ret.erase(ret.rfind(":"), 5);

    return ret;
}

std::string StreamAuth::getRtmpPushAddr(const std::string &host, const std::string &dev,
                        int time, uint32_t chn, uint16_t port) {
    _push_time = time;
    return getRtmpAddr(host, dev, "push", time, chn, port);
}

/** 如果time=0, 则使用推流地址设置的time，如果time>0, 应该判断不能大于推流地址的time */
std::string StreamAuth::getRtmpPullAddr(const std::string &host, const std::string &dev,
                        int time, uint32_t chn, uint16_t port) {
    if (!_push_time) {
        return std::string();
    }
    if (!time || time > _push_time)
        time = _push_time;

    return getRtmpAddr(host, dev, "pull", time, chn, port);
}

bool StreamAuth::availableRtmpAddr(const std::string &url) {
    //1.解析出时间，如果小于当前时间，直接返回错误，如果大于当前时间，构造出sstring
    //sstring = "rtmp://host:port/live/dev_Cchn?auth_key=method-timestamp-rand-uid-privateKey"
    //1.1 取出Timestamp 和 privateKey
    size_t pos = url.find("?auth_key=");
    if (pos == string::npos)
        return false;

    string auth_key = url.substr(pos+10);
    pos = auth_key.find("-");
    if (pos == string::npos)
        return false;

    string str = auth_key.substr(pos+1, 10);
    int nts = ::atoi(str.c_str());
    int cur_time = ::time(NULL);
    if (nts < cur_time)
        return false;

    pos = url.rfind("-");
    string private_key = url.substr(pos+1, url.size() - (pos+1));
    str.resize(0);
    str = url.substr(0, pos);

    //2.使用MD5算法算出hash-value，如果和请求中的hash-value一致则返回成功，否则返回失败
    return !private_key.compare(md5Sum(str + "-" + _key));
}


//srt
//-------------------------------------------------------------------------------------------------------------------------------------------

//srt://[host]:[port]?streamid=#!::h=[host],r=/live/[dev]_C[chn],m=publish/request,auth_key=[timestamp]-[rand]-[uid]-[privateKey]
//srt://192.168.0.15:60003?streamid=#!::h=192.168.0.15,r=/live/HDMN2DEV_C0,m=publish,auth_key=1669813114-0-0-602c8645457f82b72fce392da3814038
std::string StreamAuth::getSrtAddr(const std::string &host,
                const std::string &dev, const std::string &method,
                int time, uint32_t chn, uint16_t port) {
    ostringstream oss;
    oss << "srt://" << host << ":" << port;
    oss << "?streamid=#!::h=" << host << ",r=/live/" << dev << "_C" << chn;
    oss << ",m=" << method << ",auth_key=" << time << "-0-0-";

    //md5sum(uri+method-time+rand+uid+accessToken)
    string ret;
    ret = oss.str(); ret.append(md5Sum(string(oss.str()) + _key));
    return ret;
}

std::string StreamAuth::getSrtPushAddr(const std::string &host, const std::string &dev,
                        int time, uint16_t port, uint32_t chn) {
    _push_time = time;
    return getSrtAddr(host, dev, "publish", time, chn, port);
}

std::string StreamAuth::getSrtPullAddr(const std::string &host, const std::string &dev,
                    int time, uint16_t port, uint32_t chn) {
    if (!_push_time) {
        return std::string();
    }
    if (!time || time > _push_time)
        time = _push_time;

    return getSrtAddr(host, dev, "request", time, chn, port);
}

bool StreamAuth::availableSrtAddr(const std::string &url) {
    //1.解析出时间，如果小于当前时间，直接返回错误，如果大于当前时间，构造出sstring
    //sstring = "srt://[host]:[port]?streamid=#!::h=[host],r=/live/[dev]_C[chn],m=publish/request,auth_key=[timestamp]-[rand]-[uid]-[privateKey]"
    //1.1 取出Timestamp 和 privateKey
    size_t pos = url.find(",auth_key=");
    if (pos == string::npos)
        return false;

    string auth_key = url.substr(pos+10);
    pos = auth_key.find("-");
    if (pos == string::npos)
        return false;

    string str = auth_key.substr(0, pos);
    int nts = ::atoi(str.c_str());
    int cur_time = ::time(NULL);
    if (nts < cur_time)
        return false;

    pos = url.rfind("-");
    string private_key = url.substr(pos+1, url.size() - (pos+1));
    str.resize(0);
    str = url.substr(0, pos);

    //2.使用MD5算法算出hash-value，如果和请求中的hash-value一致则返回成功，否则返回失败
    return !private_key.compare(md5Sum(str + "-" + _key));
}

// #define AUTH_TEST
#ifdef AUTH_TEST
int main(void) {
    string accessToken("4005e85f6e0ee57739bbf0d07dbdfdd38ec78a8e");
    string host("192.168.0.15");
    string dev("HDMN2DEV");
    time_t ts = ::time(NULL) + 365*24*60*60;
    cout << "time:" << ts << endl;

    StreamAuth::Ptr auth = make_shared<StreamAuth>(accessToken);
    string pushAddr = auth->getRtmpPushAddr(host, dev, ts);
    cout << "pushAddr:" << pushAddr << endl;

    string pullAddr = auth->getRtmpPullAddr(host, dev, ts);
    cout << "pullAddr:" << pullAddr << endl;

    string auth_key;
    bool result = false, stop = false;

    while(true) {
        char input = getchar();
        switch (input) {
            case '1': {
                cout << "Check push address, Please input auth_key" << endl;
                cin >> auth_key;
                result = auth->AvailableRtmpAddr(auth_key);
                cout << "result:" << result << endl;
                break;
            }
            case '2': {
                cout << "Check pull address, Please input auth_key" << endl;
                cin >> auth_key;
                result = auth->AvailableRtmpAddr(auth_key);
                cout << "result:" << result << endl;
                break;
            }
            case 'q':
            {
                stop = true;
                break;
            }
        }

        if (stop) break;
        else usleep(100*1000);
    }

    return 0;
}
#endif
}