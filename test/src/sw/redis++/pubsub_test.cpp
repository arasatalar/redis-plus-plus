/**************************************************************************
   Copyright (c) 2017 sewenew

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
 *************************************************************************/

#include "pubsub_test.h"
#include <unordered_map>
#include <unordered_set>
#include "utils.h"

namespace sw {

namespace redis {

namespace test {

PubSubTest::PubSubTest(const ConnectionOptions &opts) : _redis(opts, _pool_opts()) {}

void PubSubTest::run() {
    _test_sub_channel();

    _test_sub_pattern();

    _test_unsubscribe();
}

ConnectionPoolOptions PubSubTest::_pool_opts() const {
    ConnectionPoolOptions opts;
    opts.size = 2;

    return opts;
}

void PubSubTest::_test_sub_channel() {
    auto sub = _redis.subscriber();

    auto msgs = {"msg1", "msg2"};
    auto channel1 = test_key("c1");
    sub.subscribe(channel1, [&msgs, &channel1](std::string channel, std::string msg) {
                static std::size_t idx = 0;
                REDIS_ASSERT(channel == channel1 && msg == *(msgs.begin() + idx),
                                "failed to test subscribe");
                ++idx;
            });

    // Consume the SUBSCRIBE message.
    sub.consume();

    for (const auto &msg : msgs) {
        _redis.publish(channel1, msg);
        sub.consume();
    }

    sub.unsubscribe(channel1);

    auto channel2 = test_key("c2");
    auto channel3 = test_key("c3");
    auto channel4 = test_key("c4");
    std::unordered_set<std::string> channels;
    sub = _redis.subscriber([&channels](Subscriber::MsgType type,
                                        OptionalString channel,
                                        long long num) {
                REDIS_ASSERT(bool(channel), "failed to test subscribe");

                if (type == Subscriber::MsgType::SUBSCRIBE) {
                    auto iter = channels.find(*channel);
                    REDIS_ASSERT(iter == channels.end(), "failed to test subscribe");
                    channels.insert(*channel);
                    REDIS_ASSERT(static_cast<std::size_t>(num) == channels.size(),
                                    "failed to test subscribe");
                } else if (type == Subscriber::MsgType::UNSUBSCRIBE) {
                    auto iter = channels.find(*channel);
                    REDIS_ASSERT(iter != channels.end(), "failed to test subscribe");
                    channels.erase(*channel);
                    REDIS_ASSERT(static_cast<std::size_t>(num) == channels.size(),
                                    "failed to test subscribe");
                } else {
                    REDIS_ASSERT(false, "Unknown message type");
                }
            });

    std::unordered_map<std::string, std::string> messages = {
        {channel2, "msg2"},
        {channel3, "msg3"},
        {channel4, "msg4"},
    };
    sub.subscribe({channel2, channel3, channel4},
                    [&messages](std::string channel, std::string msg) {
                        REDIS_ASSERT(messages.find(channel) != messages.end(),
                                        "failed to test subscribe");
                        REDIS_ASSERT(messages[channel] == msg, "failed to test subscribe");
                    });

    for (std::size_t idx = 0; idx != channels.size(); ++idx) {
        sub.consume();
    }

    for (const auto &ele : messages) {
        _redis.publish(ele.first, ele.second);
        sub.consume();
    }

    sub.unsubscribe({channel2, channel3, channel4});
}

void PubSubTest::_test_sub_pattern() {
    auto sub = _redis.subscriber();

    auto msgs = {"msg1", "msg2"};
    auto pattern1 = test_key("pattern*");
    std::string channel1 = test_key("pattern1");
    sub.psubscribe(pattern1,
                    [&msgs, &pattern1, &channel1](std::string pattern,
                                                    std::string channel,
                                                    std::string msg) {
                        static std::size_t idx = 0;
                        REDIS_ASSERT(pattern == pattern1
                                        && channel == channel1
                                        && msg == *(msgs.begin() + idx),
                                        "failed to test psubscribe");
                        ++idx;
                    });

    // Consume the PSUBSCRIBE message.
    sub.consume();

    for (const auto &msg : msgs) {
        _redis.publish(channel1, msg);
        sub.consume();
    }

    sub.unsubscribe(channel1);

    auto channel2 = test_key("pattern22");
    auto channel3 = test_key("pattern33");
    auto channel4 = test_key("pattern44");
    std::unordered_set<std::string> channels;
    sub = _redis.subscriber([&channels](Subscriber::MsgType type,
                                        OptionalString channel,
                                        long long num) {
                REDIS_ASSERT(bool(channel), "failed to test psubscribe");

                if (type == Subscriber::MsgType::PSUBSCRIBE) {
                    auto iter = channels.find(*channel);
                    REDIS_ASSERT(iter == channels.end(), "failed to test psubscribe");
                    channels.insert(*channel);
                    REDIS_ASSERT(static_cast<std::size_t>(num) == channels.size(),
                                    "failed to test psubscribe");
                } else if (type == Subscriber::MsgType::PUNSUBSCRIBE) {
                    auto iter = channels.find(*channel);
                    REDIS_ASSERT(iter != channels.end(), "failed to test psubscribe");
                    channels.erase(*channel);
                    REDIS_ASSERT(static_cast<std::size_t>(num) == channels.size(),
                                    "failed to test psubscribe");
                } else {
                    REDIS_ASSERT(false, "Unknown message type");
                }
            });

    auto pattern2 = test_key("pattern2*");
    auto pattern3 = test_key("pattern3*");
    auto pattern4 = test_key("pattern4*");
    std::unordered_map<std::string, std::string> messages = {
        {channel2, "msg2"},
        {channel3, "msg3"},
        {channel4, "msg4"},
    };
    sub.psubscribe({pattern2, pattern3, pattern4},
                    [&messages](std::string pattern, std::string /*channel*/, std::string msg) {
                        REDIS_ASSERT(messages.find(pattern) != messages.end(),
                                        "failed to test psubscribe");
                        REDIS_ASSERT(messages[pattern] == msg, "failed to test psubscribe");
                    });

    for (std::size_t idx = 0; idx != channels.size(); ++idx) {
        sub.consume();
    }

    for (const auto &ele : messages) {
        _redis.publish(ele.first, ele.second);
        sub.consume();
    }

    sub.punsubscribe({pattern2, pattern3, pattern4});
}

void PubSubTest::_test_unsubscribe() {
    auto sub = _redis.subscriber([](Subscriber::MsgType type,
                                    OptionalString channel,
                                    long long num) {
                REDIS_ASSERT(type == Subscriber::MsgType::UNSUBSCRIBE, "failed to test unsub");

                REDIS_ASSERT(!channel, "failed to test unsub");

                REDIS_ASSERT(num == 0, "failed to test unsub");
            });

    sub.unsubscribe();
    sub.consume();
}

}

}

}