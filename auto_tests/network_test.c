#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "check_compat.h"

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>

#include "../toxcore/network.h"

#include "helpers.h"

#ifndef USE_IPV6
#define USE_IPV6 1
#endif

START_TEST(test_addr_resolv_localhost)
{
#ifdef __CYGWIN__
    /* force initialization of network stack
     * normally this should happen automatically
     * cygwin doesn't do it for every network related function though
     * e.g. not for getaddrinfo... */
    net_socket(0, 0, 0);
    errno = 0;
#endif

    const char localhost[] = "localhost";
    int localhost_split = 0;

    IP ip;
    ip_init(&ip, 0); // ipv6enabled = 0

    int res = addr_resolve(localhost, &ip, nullptr);

    ck_assert_msg(res > 0, "Resolver failed: %u, %s", errno, strerror(errno));

    char ip_str[IP_NTOA_LEN];
    ck_assert_msg(ip.family == TOX_AF_INET, "Expected family TOX_AF_INET, got %u.", ip.family);
    const uint32_t loopback = get_ip4_loopback().uint32;
    ck_assert_msg(ip.ip.v4.uint32 == loopback, "Expected 127.0.0.1, got %s.",
                  ip_ntoa(&ip, ip_str, sizeof(ip_str)));

    ip_init(&ip, 1); // ipv6enabled = 1
    res = addr_resolve(localhost, &ip, nullptr);

    if (!(res & TOX_ADDR_RESOLVE_INET6)) {
        res = addr_resolve("ip6-localhost", &ip, nullptr);
        localhost_split = 1;
    }

    ck_assert_msg(res > 0, "Resolver failed: %u, %s", errno, strerror(errno));

    ck_assert_msg(ip.family == TOX_AF_INET6, "Expected family TOX_AF_INET6 (%u), got %u.", TOX_AF_INET6, ip.family);
    IP6 ip6_loopback = get_ip6_loopback();
    ck_assert_msg(!memcmp(&ip.ip.v6, &ip6_loopback, sizeof(IP6)), "Expected ::1, got %s.",
                  ip_ntoa(&ip, ip_str, sizeof(ip_str)));

    if (localhost_split) {
        printf("Localhost seems to be split in two.\n");
        return;
    }

    ip_init(&ip, 1); // ipv6enabled = 1
    ip.family = TOX_AF_UNSPEC;
    IP extra;
    ip_reset(&extra);
    res = addr_resolve(localhost, &ip, &extra);
    ck_assert_msg(res > 0, "Resolver failed: %u, %s", errno, strerror(errno));

#if USE_IPV6
    ck_assert_msg(ip.family == TOX_AF_INET6, "Expected family TOX_AF_INET6 (%u), got %u.", TOX_AF_INET6, ip.family);
    ck_assert_msg(!memcmp(&ip.ip.v6, &ip6_loopback, sizeof(IP6)), "Expected ::1, got %s.",
                  ip_ntoa(&ip, ip_str, sizeof(ip_str)));

    ck_assert_msg(extra.family == TOX_AF_INET, "Expected family TOX_AF_INET (%u), got %u.", TOX_AF_INET, extra.family);
    ck_assert_msg(extra.ip.v4.uint32 == loopback, "Expected 127.0.0.1, got %s.",
                  ip_ntoa(&ip, ip_str, sizeof(ip_str)));
#else
    ck_assert_msg(ip.family == TOX_AF_INET, "Expected family TOX_AF_INET (%u), got %u.", TOX_AF_INET, ip.family);
    ck_assert_msg(ip.ip.v4.uint32 == loopback, "Expected 127.0.0.1, got %s.",
                  ip_ntoa(&ip, ip_str, sizeof(ip_str)));
#endif
}
END_TEST

START_TEST(test_ip_equal)
{
    int res;
    IP ip1, ip2;
    ip_reset(&ip1);
    ip_reset(&ip2);

    res = ip_equal(nullptr, nullptr);
    ck_assert_msg(res == 0, "ip_equal(NULL, NULL): expected result 0, got %u.", res);

    res = ip_equal(&ip1, nullptr);
    ck_assert_msg(res == 0, "ip_equal(PTR, NULL): expected result 0, got %u.", res);

    res = ip_equal(nullptr, &ip1);
    ck_assert_msg(res == 0, "ip_equal(NULL, PTR): expected result 0, got %u.", res);

    ip1.family = TOX_AF_INET;
    ip1.ip.v4.uint32 = net_htonl(0x7F000001);

    res = ip_equal(&ip1, &ip2);
    ck_assert_msg(res == 0, "ip_equal( {TOX_AF_INET, 127.0.0.1}, {TOX_AF_UNSPEC, 0} ): "
                  "expected result 0, got %u.", res);

    ip2.family = TOX_AF_INET;
    ip2.ip.v4.uint32 = net_htonl(0x7F000001);

    res = ip_equal(&ip1, &ip2);
    ck_assert_msg(res != 0, "ip_equal( {TOX_AF_INET, 127.0.0.1}, {TOX_AF_INET, 127.0.0.1} ): "
                  "expected result != 0, got 0.");

    ip2.ip.v4.uint32 = net_htonl(0x7F000002);

    res = ip_equal(&ip1, &ip2);
    ck_assert_msg(res == 0, "ip_equal( {TOX_AF_INET, 127.0.0.1}, {TOX_AF_INET, 127.0.0.2} ): "
                  "expected result 0, got %u.", res);

    ip2.family = TOX_AF_INET6;
    ip2.ip.v6.uint32[0] = 0;
    ip2.ip.v6.uint32[1] = 0;
    ip2.ip.v6.uint32[2] = net_htonl(0xFFFF);
    ip2.ip.v6.uint32[3] = net_htonl(0x7F000001);

    ck_assert_msg(IPV6_IPV4_IN_V6(ip2.ip.v6) != 0,
                  "IPV6_IPV4_IN_V6(::ffff:127.0.0.1): expected != 0, got 0.");

    res = ip_equal(&ip1, &ip2);
    ck_assert_msg(res != 0, "ip_equal( {TOX_AF_INET, 127.0.0.1}, {TOX_AF_INET6, ::ffff:127.0.0.1} ): "
                  "expected result != 0, got 0.");

    IP6 ip6_loopback = get_ip6_loopback();
    memcpy(&ip2.ip.v6, &ip6_loopback, sizeof(IP6));
    res = ip_equal(&ip1, &ip2);
    ck_assert_msg(res == 0, "ip_equal( {TOX_AF_INET, 127.0.0.1}, {TOX_AF_INET6, ::1} ): expected result 0, got %u.", res);

    memcpy(&ip1, &ip2, sizeof(IP));
    res = ip_equal(&ip1, &ip2);
    ck_assert_msg(res != 0, "ip_equal( {TOX_AF_INET6, ::1}, {TOX_AF_INET6, ::1} ): expected result != 0, got 0.");

    ip2.ip.v6.uint8[15]++;
    res = ip_equal(&ip1, &ip2);
    ck_assert_msg(res == 0, "ip_equal( {TOX_AF_INET6, ::1}, {TOX_AF_INET6, ::2} ): expected result 0, got %u.", res);
}
END_TEST

static Suite *network_suite(void)
{
    Suite *s = suite_create("Network");

    DEFTESTCASE(addr_resolv_localhost);
    DEFTESTCASE(ip_equal);

    return s;
}

int main(void)
{
    setvbuf(stdout, nullptr, _IONBF, 0);
    srand((unsigned int) time(nullptr));

    Suite *network = network_suite();
    SRunner *test_runner = srunner_create(network);
    int number_failed = 0;

    srunner_run_all(test_runner, CK_NORMAL);
    number_failed = srunner_ntests_failed(test_runner);

    srunner_free(test_runner);

    return number_failed;
}
