// https://stackoverflow.com/a/49441637/23298230

#pragma once
#include <spdlog/spdlog.h>
#include <spdlog/sinks/ansicolor_sink.h>

#include <utility>

namespace logging {

    spdlog::logger & instance()
    {
        auto sink =
                std::make_shared<spdlog::sinks::ansicolor_stdout_sink_mt>();
        decltype(sink) sinks[] = {sink};
        static spdlog::logger logger(
                "console", std::begin( sinks ), std::end( sinks ) );
        return logger;
    }

    class log_context
    {
    public:
        log_context( const char *         name,
                     std::string   scope_name )
                : name_ ( name       )
                , scope_(std::move( scope_name ))
        {}

        [[nodiscard]] const char * name() const
        { return name_; }

        [[nodiscard]] const char * scope() const
        { return scope_.c_str(); }

    private:
        const char *  name_;
        std::string   scope_;
    };

    class log_statement
    {
    public:
        log_statement( spdlog::logger &           logger,
                       spdlog::level::level_enum  level,
                       const log_context &        context )
                : logger_ ( logger  )
                , level_  ( level   )
                , context_( context )
        {}

        template<class T, class... U>
        void operator()( const T &  t, U&&...  u )
        {
            std::string  fmt = std::string( "[{}] " ) + t;
            logger_.log(
                    level_,
                    fmt.c_str(),
                    context_.scope(),
                    std::forward<U>( u )... );
        }

    private:
        spdlog::logger &           logger_;
        spdlog::level::level_enum  level_;
        const log_context &        context_;
    };

} // namespace logging

// Helpers for walking up the lexical scope chain.
template<class T, class Prev = typename T::prev>
struct chain
{
    static std::string get()
    {
        return (chain<Prev, typename Prev::prev>::get() + ".")
               + T::name();
    }
};

template<class T>
struct chain<T, void*>
{
    static std::string get()
    {
        return T::name();
    }
};

#define LOGGER ::logging::instance()

#define CHECK_LEVEL( level_name ) \
    LOGGER.should_log( ::spdlog::level::level_name )

#define CHECK_AND_LOG( level_name )      \
    if ( !CHECK_LEVEL( level_name ) ) {} \
    else                                 \
        ::logging::log_statement(        \
            LOGGER,                      \
            ::spdlog::level::level_name, \
            __log_context__::context() )

#define LOG_TRACE CHECK_AND_LOG( trace )
#define LOG_DEBUG CHECK_AND_LOG( debug )
#define LOG_INFO CHECK_AND_LOG( info )
#define LOG_WARNING CHECK_AND_LOG( warn )
#define LOG_ERROR CHECK_AND_LOG( err )
#define LOG_CRITICAL CHECK_AND_LOG( critical )

#define LOG_CONTEXT_IMPL(prev_type,name_)            \
struct __log_context__                               \
{                                                    \
    typedef prev_type prev;                          \
    static const char * name() { return name_; }     \
    static ::logging::log_context  context()         \
    {                                                \
        return ::logging::log_context(               \
            name(), chain<__log_context__>::get() ); \
    }                                                \
};                                                   \
static __log_context__ __log_context_var__

#define LOG_CONTEXT(name_) \
    LOG_CONTEXT_IMPL(decltype(__log_context_var__),name_)

#define ROOT_CONTEXT(name_) \
    LOG_CONTEXT_IMPL(void*,name_)

// We include the root definition here to ensure that
// __log_context_var__ is always defined for any uses of
// LOG_CONTEXT.
ROOT_CONTEXT( "global" );