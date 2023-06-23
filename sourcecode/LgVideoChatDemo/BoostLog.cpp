#include "BoostLog.h"

void InitLogging()
{
    // �α� ���� ����
    logging::register_simple_formatter_factory<logging::trivial::severity_level, char>("Severity");

    logging::add_console_log(
        std::cout,
        keywords::format = expr::format("%1% [%2%] <%3%> %4%")
        % expr::format_date_time< boost::posix_time::ptime >("TimeStamp", "%Y-%m-%d, %H:%M:%S.%f")
        % expr::format_date_time< attrs::timer::value_type >("Uptime", "%O:%M:%S")
        % expr::attr<logging::trivial::severity_level>("Severity")
        % expr::message
    );

    // �α� ���� ����
    logging::add_file_log(
        keywords::file_name = "LgVideoChat_%N.log",
        keywords::open_mode = std::ios_base::app,                           // ���� �α� ���Ͽ� �̾ �߰�
        keywords::rotation_size = 10 * 1024 * 1024,                          /*< rotate files every 10 MiB... >*/
        keywords::auto_flush = true,
        keywords::format = expr::format("%1% [%2%] <%3%> %4%")
        % expr::format_date_time< boost::posix_time::ptime >("TimeStamp", "%Y-%m-%d, %H:%M:%S.%f")
        % expr::format_date_time< attrs::timer::value_type >("Uptime", "%O:%M:%S")
        % expr::attr<logging::trivial::severity_level>("Severity")
        % expr::message
    );

    // Also let's add some commonly used attributes, like timestamp and record counter.
    logging::add_common_attributes();
    logging::core::get()->add_global_attribute("Uptime", attrs::timer());

    // �α� ���� ���� (���⼭�� ��� �α� ������ ���)
    logging::core::get()->set_filter(logging::trivial::severity >= logging::trivial::trace);
}

