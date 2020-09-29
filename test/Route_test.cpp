#include "Route.h"

#include <gtest/gtest.h>

#include <boost/beast.hpp>

#include <vector>

namespace beast = boost::beast;
namespace http = boost::beast::http;

/*
 * Create a simple request with basic options, still need to call
 * prepare_payload().
 */
static http::request<http::string_body> create_basic_request()
{
    http::request<http::string_body> req;
    req.method(http::verb::get);
    req.target("/");
    req.set(http::field::content_type, "text/plain");
    req.set(http::field::accept, "*/*");
    req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
    req.set(http::field::host, "UnitTesting.blah");

    return req;
}

TEST(Route, FromABoostRequest)
{
    auto req = create_basic_request();
    req.body() = "Test";
    req.prepare_payload();
    Request pub_req = from_boost_request(req);
    EXPECT_EQ(pub_req.headers().method(), std::string("GET"));
    EXPECT_EQ(pub_req.headers().version(), 11);
    std::string header_val;
    EXPECT_TRUE(pub_req.headers().get_header("content-type", header_val));
    EXPECT_EQ(header_val, "text/plain");
    EXPECT_TRUE(pub_req.headers().get_header("accept", header_val));
    EXPECT_EQ(header_val, "*/*");
    EXPECT_TRUE(pub_req.headers().get_header("user-agent", header_val));
    EXPECT_EQ(header_val, BOOST_BEAST_VERSION_STRING);
    EXPECT_TRUE(pub_req.headers().get_header("host", header_val));
    EXPECT_EQ(header_val, "UnitTesting.blah");
    EXPECT_EQ(pub_req.body(), "Test");
}

TEST(Route, ToABoostResponse)
{
    Response pub_resp(200, R"~({"foo": "bar"})~", "application/json");
    auto resp = to_boost_response(pub_resp);
    EXPECT_EQ(resp.result_int(), 200);
    EXPECT_EQ(resp.body(), std::string(R"~({"foo": "bar"})~"));
    auto it = resp.find(boost::beast::http::field::content_type);
    EXPECT_FALSE(it == resp.end());
    EXPECT_EQ(it->value(), std::string("application/json"));
    EXPECT_EQ(resp.version(), 11);
}
