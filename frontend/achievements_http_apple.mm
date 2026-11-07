// RetroAchievements' internet requests on the Mac and on iPhone and iPad (achievements_http.cpp
// has the other platforms'): NSURLSession, part of the system, waited for on the requests' own
// thread (achievements.cpp's worker), so nothing else waits.
#import <Foundation/Foundation.h>
#include <string>
#include "net_http.h"

namespace Achievements
{

const char* SystemName()
{
#if TARGET_OS_IPHONE
    return "iOS";
#else
    return "macOS";
#endif
}

bool Available() { return true; }

bool HttpRequest(const HttpCall& call, int& status, std::string& body, std::string& error)
{
    const std::string& url = call.url;
    const std::string& post = call.body;
    const std::string& userAgent = call.userAgent;
    @autoreleasepool
    {
        NSURL* address = [NSURL URLWithString:[NSString stringWithUTF8String:url.c_str()]];
        if (!address)
        {
            error = "bad address";
            return false;
        }
        NSMutableURLRequest* request = [NSMutableURLRequest requestWithURL:address
                                                               cachePolicy:NSURLRequestReloadIgnoringLocalCacheData
                                                           timeoutInterval:(NSTimeInterval)call.timeout];
        [request setValue:[NSString stringWithUTF8String:userAgent.c_str()] forHTTPHeaderField:@"User-Agent"];
        request.HTTPMethod = [NSString stringWithUTF8String:call.method.c_str()];
        for (const std::string& h : call.headers)
        {
            const size_t colon = h.find(':');
            if (colon == std::string::npos) continue;
            size_t v = colon + 1;
            while (v < h.size() && h[v] == ' ') v++;
            [request setValue:[NSString stringWithUTF8String:h.substr(v).c_str()]
                forHTTPHeaderField:[NSString stringWithUTF8String:h.substr(0, colon).c_str()]];
        }
        if (!post.empty() || call.method == "POST" || call.method == "PUT" || call.method == "PATCH")
            request.HTTPBody = [NSData dataWithBytes:post.data() length:post.size()];
        // (What comes back is copied out inside the handler: with or without ARC, nothing of it
        // has to outlive the handler.)
        std::string* gotBody = &body;
        const size_t limit = call.limit;
        std::string* gotError = &error;
        __block NSInteger code = 0;
        __block bool failed = false;
        gotBody->clear();
        dispatch_semaphore_t done = dispatch_semaphore_create(0);
        NSURLSessionDataTask* task = [[NSURLSession sharedSession]
            dataTaskWithRequest:request
              completionHandler:^(NSData* data, NSURLResponse* response, NSError* err) {
                if (err)
                {
                    failed = true;
                    *gotError = std::string("no connection (") + (err.localizedDescription.UTF8String ?: "") + ")";
                }
                else
                {
                    if (data) gotBody->assign((const char*)data.bytes, data.length);
                    if (gotBody->size() > limit) gotBody->resize(limit);
                    if ([response isKindOfClass:[NSHTTPURLResponse class]]) code = ((NSHTTPURLResponse*)response).statusCode;
                }
                dispatch_semaphore_signal(done);
              }];
        [task resume];
        dispatch_semaphore_wait(done, DISPATCH_TIME_FOREVER);
#if !__has_feature(objc_arc)
        dispatch_release(done);
#endif
        if (failed) return false;
        status = (int)code;
        return true;
    }
}


bool HttpPost(const std::string& url, const std::string& post, const std::string& contentType,
              const std::string& userAgent, int& status, std::string& body, std::string& error)
{
    HttpCall c;
    c.method = post.empty() ? "GET" : "POST";
    c.url = url;
    if (!post.empty()) c.headers.push_back("Content-Type: " + contentType);
    c.body = post;
    c.userAgent = userAgent;
    return HttpRequest(c, status, body, error);
}

}
