#ifndef __LIBCATNAP_ROUTE_H__
#define __LIBCATNAP_ROUTE_H__

#include <string>
#include <string_view>
#include <map>

#include <iostream>

template<bool isRequest>
class Headers;

template<>
class Headers<true>
{
        std::map<std::string, std::string> _headers;
        unsigned _version;
        std::string _method;
        std::string _route;
        std::map<std::string, std::string> _params;

        using iterator = std::map<std::string, std::string>::iterator;

    public:
        /**
         * Getter and Setter for the HTTP version.
         */
        unsigned version() const
        {
            return this->_version;
        }
        void version(const unsigned value)
        {
            this->_version = value;
        }

        /**
         * Getter and Setter for the HTTP request route.
         */
        std::string method() const
        {
            return this->_method;
        }
        void method(const std::string value)
        {
            this->_method = value;
        }

        /**
         * Getter and Setter for the HTTP request route.
         */
        std::string route() const
        {
            return this->_route;
        }
        void route(const std::string value)
        {
            this->_route = value;
        }

        /**
         * Add a header.
         *
         * @param [in] header
         * @param [in] value
         */
        void add_header(const std::string header, const std::string value)
        {
            std::string newheader = this->str_to_lower(header);
            this->_headers[newheader] = value;
        }
        /**
         * Get a header.
         *
         * @param [in] header
         * @param [out] value
         *
         * @returns
         *      (true) If the header was found and value was set.
         *      (false) if the header was not found.
         */
        bool get_header(const std::string header, std::string &value)
        {
            std::string newheader = this->str_to_lower(header);
            for (auto it : this->_headers) {
                if (it.first == newheader) {
                    value = it.second;
                    return true;
                }
            }
            return false;
        }

        iterator headers_begin()
        {
            return this->_headers.begin();
        }
        iterator headers_end()
        {
            return this->_headers.end();
        }

        iterator params_begin()
        {
            return this->_params.begin();
        }
        iterator params_end()
        {
            return this->_params.end();
        }

        /**
         * Add a param.
         *
         * @param [in] param
         * @param [in] value
         */
        void add_param(const std::string param, const std::string value)
        {
            std::string newparam = this->str_to_lower(param);
            this->_params[newparam] = value;
        }
        /**
         * Get a param.
         *
         * @param [in] param
         * @param [out] value
         *
         * @returns
         *      (true) If the param was found and value was set.
         *      (false) if the param was not found.
         */
        bool get_param(const std::string param, std::string &value)
        {
            std::string newparam = this->str_to_lower(param);
            for (auto it : this->_params) {
                if (it.first == newparam) {
                    value = it.second;
                    return true;
                }
            }
            return false;
        }

    private:
        std::string str_to_lower (std::string s)
        {
            std::transform(s.begin(), s.end(), s.begin(),
                [](unsigned char c) -> unsigned char {
                    return std::tolower(c);
                });
            return s;
        };
};

template<>
class Headers<false>
{
        std::map<std::string, std::string> _headers;
        unsigned _version = 11;
        int _status_code = 200;

        using iterator = std::map<std::string, std::string>::iterator;

    public:
        /**
         * Getter and Setter for the HTTP version.
         */
        unsigned version() const
        {
            return this->_version;
        }
        void version(const unsigned value)
        {
            this->_version = value;
        }

        /**
         * Getter and Setter for the HTTP status code.
         */
        unsigned status_code() const
        {
            return this->_status_code;
        }
        void status_code(const unsigned value)
        {
            this->_status_code = value;
        }

        /**
         * Add a header.
         *
         * @param [in] header
         * @param [in] value
         */
        void add_header(const std::string header, const std::string value)
        {
            std::string newheader = this->str_to_lower(header);
            this->_headers[newheader] = value;
        }
        /**
         * Get a header.
         *
         * @param [in] header
         * @param [out] value
         *
         * @returns
         *      (true) If the header was found and value was set.
         *      (false) if the header was not found.
         */
        bool get_header(const std::string header, std::string &value)
        {
            std::string newheader = this->str_to_lower(header);
            for (auto it : this->_headers) {
                if (it.first == newheader) {
                    value = it.second;
                    return true;
                }
            }
            return false;
        }

        iterator headers_begin()
        {
            return this->_headers.begin();
        }
        iterator headers_end()
        {
            return this->_headers.end();
        }

    private:
        std::string str_to_lower (std::string s)
        {
            std::transform(s.begin(), s.end(), s.begin(),
                [](unsigned char c) -> unsigned char {
                    return std::tolower(c);
                });
            return s;
        };
};

template <bool isRequest>
class BaseMessage {
        Headers<isRequest> _headers;
        std::string _body;

    public:
        enum class BodyType {
            STRING, FILE_
        };

    public:
        BaseMessage<isRequest>()
        {
        }

        BaseMessage<isRequest>(int status_code)
        {
            this->_headers.status_code(status_code);
        }

        BaseMessage<isRequest>(int status_code, const std::string &message,
                const std::string &content_type = "text/plain")
        {
            this->_headers.status_code(status_code);
            this->_body = message;
            this->headers().add_header("Content-Type", content_type);
        }

    public:

        Headers<isRequest> & headers() & noexcept
        {
            return this->_headers;
        }
        Headers<isRequest> && headers() && noexcept
        {
            return std::move(this->_headers);
        }
        Headers<isRequest> const & headers() const& noexcept
        {
            return this->_headers;
        }
        void headers(const Headers<isRequest> &headers)
        {
            this->_headers = headers;
        }

        /**
         * Set or get the body of the message
         */
        std::string & body()
        {
            return this->_body;
        }
        void body(const std::string &body)
        {
            this->_body = body;
        }
};

using Request = BaseMessage<true>;
using Response = BaseMessage<false>;

class Route {
    public:
        typedef Response (*route_cb_t) (const Request &);

    protected:
        std::string _route;
        route_cb_t _callback;
        std::string _method;

    public:
        explicit Route(std::string route, route_cb_t cb, std::string method = "GET");
        virtual ~Route();

    public:
        std::string method() const;
        std::string route() const;
        route_cb_t callback() const;
};

#include <boost/beast.hpp>

class PrivateRoute {
    protected:
        Route _route;

    public:
        explicit PrivateRoute(Route route);
        virtual ~PrivateRoute();

    public:
        auto call_callback(boost::beast::http::request<
                boost::beast::http::string_body> &request);

        bool operator==(const Route &rhs) const;
};

template <typename T>
static inline Request from_boost_request(
        const boost::beast::http::request<T> &request)
{
    auto verb_to_str = [](const boost::beast::http::verb &verb)
        -> std::string { return std::string(to_string(verb)); };
    boost::beast::http::request<boost::beast::http::string_body> test_req;
    Request req;
    req.headers().version(request.version());
    req.headers().method(request.method_string().to_string());
    req.headers().route(request.target().to_string());
    for (boost::beast::http::fields::iterator it=request.base().begin()
            ; it!=request.base().end()
            ; it++) {
        if (it->name() == boost::beast::http::field::version) {
            continue;
        }
        req.headers().add_header(
            std::string(it->name_string()),
            std::string(it->value()));
    }
    req.body() = request.body();
    return req;
}

static inline auto to_boost_response(Response &response)
{
    boost::beast::http::response<boost::beast::http::string_body> resp;
    resp.result(response.headers().status_code());
    resp.version(response.headers().version());
    resp.body() = response.body();
    for (auto it = response.headers().headers_begin()
            ; it != response.headers().headers_end()
            ; it++) {
        resp.insert(it->first, it->second);
    }
    return resp;
}

#endif /* __LIBCATNAP_ROUTE_H__ */
