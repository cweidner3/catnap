#include "HttpHandler.h"

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio.hpp>
#include <boost/config.hpp>

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <stdexcept>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = boost::beast::http;    // from <boost/beast/http.hpp>

//------------------------------------------------------------------------------

//------------------------------------------------------------------------------

/*
 * Return a reasonable mime type based on the extension of a file.
 */
static beast::string_view mime_type(beast::string_view path)
{
    using beast::iequals;
    auto const ext = [&path]
    {
        auto const pos = path.rfind(".");
        if(pos == beast::string_view::npos) {
            return beast::string_view{};
        }
        return path.substr(pos);
    }();
    if(iequals(ext, ".htm"))  return "text/html";
    if(iequals(ext, ".html")) return "text/html";
    if(iequals(ext, ".php"))  return "text/html";
    if(iequals(ext, ".css"))  return "text/css";
    if(iequals(ext, ".txt"))  return "text/plain";
    if(iequals(ext, ".js"))   return "application/javascript";
    if(iequals(ext, ".json")) return "application/json";
    if(iequals(ext, ".xml"))  return "application/xml";
    if(iequals(ext, ".swf"))  return "application/x-shockwave-flash";
    if(iequals(ext, ".flv"))  return "video/x-flv";
    if(iequals(ext, ".png"))  return "image/png";
    if(iequals(ext, ".jpe"))  return "image/jpeg";
    if(iequals(ext, ".jpeg")) return "image/jpeg";
    if(iequals(ext, ".jpg"))  return "image/jpeg";
    if(iequals(ext, ".gif"))  return "image/gif";
    if(iequals(ext, ".bmp"))  return "image/bmp";
    if(iequals(ext, ".ico"))  return "image/vnd.microsoft.icon";
    if(iequals(ext, ".tiff")) return "image/tiff";
    if(iequals(ext, ".tif"))  return "image/tiff";
    if(iequals(ext, ".svg"))  return "image/svg+xml";
    if(iequals(ext, ".svgz")) return "image/svg+xml";
    return "application/text";
}

/*
 * Append an HTTP rel-path to a local filesystem path.
 * The returned path is normalized for the platform.
 */
static std::string path_cat(beast::string_view base, beast::string_view path)
{
    if(base.empty()) {
        return path.to_string();
    }
    std::string result = base.to_string();
#if BOOST_MSVC
    char constexpr path_separator = '\\';
    if(result.back() == path_separator) {
        result.resize(result.size() - 1);
    }
    result.append(path.data(), path.size());
    for(auto& c : result) {
        if(c == '/') {
            c = path_separator;
        }
    }
#else
    char constexpr path_separator = '/';
    if(result.back() == path_separator) {
        result.resize(result.size() - 1);
    }
    result.append(path.data(), path.size());
#endif
    return result;
}

/*
 * This function produces an HTTP response for the given
 * request. The type of the response object depends on the
 * contents of the request, so the interface requires the
 * caller to pass a generic lambda for receiving the response.
 */
template<class Body, class Allocator, class Send>
static void handle_request(beast::string_view doc_root,
        http::request<Body, http::basic_fields<Allocator>>&& req,
        Send&& send)
{
    // Returns a bad request response
    auto const bad_request =
    [&req](beast::string_view why)
    {
        http::response<http::string_body> res{http::status::bad_request, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = why.to_string();
        res.prepare_payload();
        return res;
    };

    // Returns a not found response
    auto const not_found =
    [&req](beast::string_view target)
    {
        http::response<http::string_body> res{http::status::not_found, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = "The resource '" + target.to_string() + "' was not found.";
        res.prepare_payload();
        return res;
    };

    // Returns a server error response
    auto const server_error =
    [&req](beast::string_view what)
    {
        http::response<http::string_body> res{http::status::internal_server_error, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = "An error occurred: '" + what.to_string() + "'";
        res.prepare_payload();
        return res;
    };

    // Make sure we can handle the method
    // TODO Modify this to allow more methods
    if( req.method() != http::verb::get &&
        req.method() != http::verb::head) {
        return send(bad_request("Unknown HTTP-method"));
    }

    // Request path must be absolute and not contain "..".
    if( req.target().empty() ||
        req.target()[0] != '/' ||
        req.target().find("..") != beast::string_view::npos) {
        return send(bad_request("Illegal request-target"));
    }

    // TODO BEGIN Replace with callback call

    // Build the path to the requested file
    std::string path = path_cat(doc_root, req.target());
    if(req.target().back() == '/') {
        path.append("index.html");
    }

    // Attempt to open the file
    beast::error_code ec;
    http::file_body::value_type body;
    body.open(path.c_str(), beast::file_mode::scan, ec);

    // Handle the case where the file doesn't exist
    if(ec == boost::system::errc::no_such_file_or_directory) {
        return send(not_found(req.target()));
    }

    // Handle an unknown error
    if(ec) {
        return send(server_error(ec.message()));
    }

    // Cache the size since we need it after the move
    auto const size = body.size();

    // Respond to HEAD request
    if(req.method() == http::verb::head)
    {
        http::response<http::empty_body> res{http::status::ok, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, mime_type(path));
        res.content_length(size);
        res.keep_alive(req.keep_alive());
        return send(std::move(res));
    }

    // Respond to GET request
    http::response<http::file_body> res{
        std::piecewise_construct,
        std::make_tuple(std::move(body)),
        std::make_tuple(http::status::ok, req.version())};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, mime_type(path));
    res.content_length(size);
    res.keep_alive(req.keep_alive());
    return send(std::move(res));

    // TODO END Replace with callback call
}

//------------------------------------------------------------------------------

// Report a failure
static void fail(boost::system::error_code ec, char const* what)
{
    std::cerr << what << ": " << ec.message() << "\n";
}

/*
 * Handles an HTTP server connection
 */
template <typename T>
class Session : public std::enable_shared_from_this<Session<T>>
{

    static_assert(
            asio::is_class<asio::ip::tcp>::value
            || asio::is_class<asio::local::stream_protocol>::value,
            "Expecting local or TCP stream socket for Listener");

    // This is the C++11 equivalent of a generic lambda.
    // The function object is used to send an HTTP message.
    struct send_lambda
    {
        Session<T> &self_;

        explicit send_lambda(Session<T>& self)
            : self_(self)
        {
        }

        template<bool isRequest, class Body, class Fields>
        void operator()(http::message<isRequest, Body, Fields>&& msg) const
        {
            // The lifetime of the message has to extend
            // for the duration of the async operation so
            // we use a shared_ptr to manage it.
            auto sp = std::make_shared<
                http::message<isRequest, Body, Fields>>(std::move(msg));

            // Store a type-erased version of the shared
            // pointer in the class to keep it alive.
            self_.res_ = sp;

            // Write the response
            http::async_write(
                self_.socket_,
                *sp,
                asio::bind_executor(
                    self_.strand_,
                    std::bind(
                        &Session::on_write,
                        self_.shared_from_this(),
                        std::placeholders::_1,
                        std::placeholders::_2,
                        sp->need_eof())));
        }
    };

    asio::basic_stream_socket<T> socket_;
    asio::strand<asio::io_context::executor_type> strand_;
    beast::flat_buffer buffer_;
    std::shared_ptr<std::string const> doc_root_;
    http::request<http::string_body> req_;
    std::shared_ptr<void> res_;
    send_lambda lambda_;

public:
    // Take ownership of the socket
    explicit Session(asio::basic_stream_socket<T> socket,
        std::shared_ptr<std::string const> const& doc_root)
            : socket_(std::move(socket)), strand_(socket_.get_executor()),
            doc_root_(doc_root), lambda_(*this)
    {
    }

    // Start the asynchronous operation
    void run()
    {
        do_read();
    }

    void do_read()
    {
        // Make the request empty before reading,
        // otherwise the operation behavior is undefined.
        req_ = {};

        // Read a request
        http::async_read(socket_, buffer_, req_,
            asio::bind_executor(
                strand_,
                std::bind(
                    &Session::on_read,
                    this->shared_from_this(),
                    std::placeholders::_1,
                    std::placeholders::_2)));
    }

    void on_read( boost::system::error_code ec, std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        // This means they closed the connection
        if(ec == http::error::end_of_stream) {
            return do_close();
        }

        if(ec) {
            return fail(ec, "read");
        }

        // Send the response
        handle_request(*doc_root_, std::move(req_), lambda_);
    }

    void on_write(boost::system::error_code ec, std::size_t bytes_transferred,
            bool close)
    {
        boost::ignore_unused(bytes_transferred);

        if(ec) {
            return fail(ec, "write");
        }

        if(close)
        {
            // This means we should close the connection, usually because
            // the response indicated the "Connection: close" semantic.
            return do_close();
        }

        // We're done with the response so delete it
        res_ = nullptr;

        // Read another request
        do_read();
    }

    void do_close()
    {
        // Send a TCP shutdown
        boost::system::error_code ec;
        if constexpr(asio::is_class<asio::ip::tcp>::value) {
            socket_.shutdown(asio::ip::tcp::socket::shutdown_send, ec);
        }

        // At this point the connection is closed gracefully
    }
};

//------------------------------------------------------------------------------

/*
 * Accepts incoming connections and launches the sessions.
 */
template <typename T>
class Listener : public std::enable_shared_from_this<Listener<T>>
{
    asio::basic_socket_acceptor<T> acceptor_;
    asio::basic_stream_socket<T> socket_;
    std::shared_ptr<std::string const> doc_root_;

    // Ensure we pass the correct types, helps with debuging issues
    static_assert(
            asio::is_class<asio::ip::tcp>::value
            || asio::is_class<asio::local::stream_protocol>::value,
            "Expecting local or TCP stream socket for Listener");

public:
    Listener(
        asio::io_context& ioc,
        asio::basic_socket_acceptor<T> acceptor,
        std::shared_ptr<std::string const> const& doc_root)
        : acceptor_(std::move(acceptor)), socket_(ioc), doc_root_(doc_root)
    {
    }

    // Start accepting incoming connections
    void run()
    {
        if(! acceptor_.is_open())
            return;
        do_accept();
    }

    void do_accept()
    {
        acceptor_.async_accept(
            socket_,
            std::bind(
                &Listener<T>::on_accept,
                this->shared_from_this(),
                std::placeholders::_1));
    }

    void on_accept(boost::system::error_code ec)
    {
        if(ec)
        {
            fail(ec, "accept");
        }
        else
        {
            // Create the Session and run it
            std::make_shared<Session<T>>(
                std::move(socket_),
                doc_root_)->run();
        }

        // Accept another connection
        do_accept();
    }
};

//------------------------------------------------------------------------------

/*
 * Use the provided info and io context to create a new TCP acceptor and
 * create a listener.
 */
bool setup_tcp_acceptor(asio::io_context &ioc,
        const std::shared_ptr<std::string> doc_root,
        asio::ip::address address, unsigned short port)
{
    boost::system::error_code ec;

    asio::ip::tcp::endpoint endpoint{address, port};
    asio::ip::tcp::acceptor acceptor(ioc);

    // Open the acceptor
    acceptor.open(endpoint.protocol(), ec);
    if(ec)
    {
        fail(ec, "open");
        return false;
    }

    // Allow address reuse
    acceptor.set_option(asio::socket_base::reuse_address(true), ec);
    if(ec)
    {
        fail(ec, "set_option");
        return false;
    }

    // Bind to the server address
    acceptor.bind(endpoint, ec);
    if(ec)
    {
        fail(ec, "bind");
        return false;
    }

    // Start listening for connections
    acceptor.listen(
        asio::socket_base::max_listen_connections, ec);
    if(ec)
    {
        fail(ec, "listen");
        return false;
    }

    std::make_shared<Listener<asio::ip::tcp>>(
        ioc,
        std::move(acceptor),
        doc_root)->run();

    return true;
}

/*
 * Use the provided info and io context to create a new UNIX acceptor and
 * create a listener.
 */
bool setup_unix_acceptor(asio::io_context &ioc,
        const std::shared_ptr<std::string> doc_root,
        std::string path, bool is_anonymous)
{
    boost::system::error_code ec;

    if (is_anonymous) {
        throw std::invalid_argument("Anonymous Unix Sockets not yet supported");
    }

    asio::local::stream_protocol::endpoint endpoint(path);
    asio::local::stream_protocol::acceptor acceptor(ioc);

    // Open the acceptor
    acceptor.open(endpoint.protocol(), ec);
    if(ec)
    {
        fail(ec, "open");
        return false;
    }

    // Allow address reuse
    acceptor.set_option(asio::socket_base::reuse_address(true), ec);
    if(ec)
    {
        fail(ec, "set_option");
        return false;
    }

    // Bind to the server address
    acceptor.bind(endpoint, ec);
    if(ec)
    {
        fail(ec, "bind");
        return false;
    }

    // Start listening for connections
    acceptor.listen(
        asio::socket_base::max_listen_connections, ec);
    if(ec)
    {
        fail(ec, "listen");
        return false;
    }

    std::make_shared<Listener<asio::local::stream_protocol>>(
        ioc,
        std::move(acceptor),
        doc_root)->run();

    return true;
}

