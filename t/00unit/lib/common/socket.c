#ifndef LOGGING_H
#define LOGGING_H
#include "logging.h"
#endif

/*
 * Copyright (c) 2015 DeNA Co., Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */
#include <stdlib.h>
#include "../../test.h"
#include "../../../../lib/common/socket.c"

static void test_on_alpn_select(void)
{
    static const h2o_iovec_t protocols[] = {{H2O_STRLIT("h2")}, {H2O_STRLIT("h2-16")}, {H2O_STRLIT("h2-14")}, {NULL}};
    const unsigned char *out;
    unsigned char outlen;
    int ret;

    ret = on_alpn_select(NULL, &out, &outlen, (const unsigned char *)H2O_STRLIT("\3foo"), (void *)protocols);
    ok(ret == SSL_TLSEXT_ERR_NOACK);

    ret = on_alpn_select(NULL, &out, &outlen, (const unsigned char *)H2O_STRLIT("\2h2"), (void *)protocols);
    ok(ret == SSL_TLSEXT_ERR_OK);
    ok(h2o_memis(out, outlen, H2O_STRLIT("h2")));

    ret = on_alpn_select(NULL, &out, &outlen, (const unsigned char *)H2O_STRLIT("\5h2-14\5h2-16\2h2"), (void *)protocols);
    ok(ret == SSL_TLSEXT_ERR_OK);
    ok(h2o_memis(out, outlen, H2O_STRLIT("h2")));

    ret = on_alpn_select(NULL, &out, &outlen, (const unsigned char *)H2O_STRLIT("\5h2-14\5h2-16"), (void *)protocols);
    ok(ret == SSL_TLSEXT_ERR_OK);
    ok(h2o_memis(out, outlen, H2O_STRLIT("h2-16")));
}

static void test_sliding_counter(void)
{
    h2o_sliding_counter_t counter = {0};
    size_t i;

    h2o_sliding_counter_start(&counter, 100);
    h2o_sliding_counter_stop(&counter, 80 + 100);
    ok(counter.average == 10);

    for (i = 0; i != 7; ++i) {
        h2o_sliding_counter_start(&counter, 1);
        h2o_sliding_counter_stop(&counter, 81);
    }
    ok(counter.average == 80);

    h2o_sliding_counter_start(&counter, 1000);
    h2o_sliding_counter_stop(&counter, 1000 + 1000 * 8 - 80 * 7);
    ok(counter.average == 1000);

    for (i = 0; i != 8; ++i) {
        h2o_sliding_counter_start(&counter, 1);
        h2o_sliding_counter_stop(&counter, 11);
    }
    ok(counter.average == 10);
}

static struct {
    struct {
        int ret;
    } fetch_tcp_info;
    struct {
        int ret;
        unsigned cur;
        size_t call_cnt;
    } minimize_notsent_lowat;
    struct {
        unsigned long ret;
    } get_cipher;
} cb_ret_vars;

static int test_adjust_notsent_lowat(h2o_socket_t *sock, unsigned notsent_lowat)
{
    cb_ret_vars.minimize_notsent_lowat.cur = notsent_lowat;
    ++cb_ret_vars.minimize_notsent_lowat.call_cnt;
    return cb_ret_vars.minimize_notsent_lowat.ret;
}

static void test_prepare_for_latency_optimization(void)
{
    struct st_h2o_socket_ssl_t sock_ssl = {.record_overhead = 5 + 8 + 16 /* GCM overhead */};
    h2o_socket_t sock = {NULL, &sock_ssl};
    h2o_socket_latency_optimization_conditions_t cond = {UINT_MAX, 10, 65535};

    /* option disabled, or if rtt is too small */
    memset(&sock._latency_optimization, 0, sizeof(sock._latency_optimization));
    memset(&cb_ret_vars, 0, sizeof(cb_ret_vars));
    cb_ret_vars.fetch_tcp_info.ret = 0;
    cb_ret_vars.get_cipher.ret = TLS1_CK_RSA_WITH_AES_128_GCM_SHA256;
    prepare_for_latency_optimized_write(&sock, &cond, 50000 /* rtt */, 1400 /* mss */, 10 /* cwnd_size */, 6 /* cwnd_avail */, 4,
                                        test_adjust_notsent_lowat);
    ok(sock._latency_optimization.state == H2O_SOCKET_LATENCY_OPTIMIZATION_STATE_DISABLED);
    ok(sock._latency_optimization.suggested_tls_payload_size == SIZE_MAX);
    ok(sock._latency_optimization.suggested_write_size == SIZE_MAX);
    ok(cb_ret_vars.minimize_notsent_lowat.call_cnt == 0);

    /* trigger optimiziation */
    memset(&sock._latency_optimization, 0, sizeof(sock._latency_optimization));
    cond.min_rtt = 25; /* 25 ms */
    prepare_for_latency_optimized_write(&sock, &cond, 50000 /* rtt */, 1400 /* mss */, 10 /* cwnd_size */, 6 /* cwnd_avail */, 4,
                                        test_adjust_notsent_lowat);
    ok(sock._latency_optimization.state == H2O_SOCKET_LATENCY_OPTIMIZATION_STATE_DETERMINED);
    ok(sock._latency_optimization.suggested_tls_payload_size == 1400 - (5 + 8 + 16));
    ok(sock._latency_optimization.suggested_write_size == (1400 - (5 + 8 + 16)) * (10 - 5 + 1));
    ok(cb_ret_vars.minimize_notsent_lowat.call_cnt == 1);
    ok(cb_ret_vars.minimize_notsent_lowat.cur == 1);

    /* recalculate with an updated cwnd,unacked */
    {  // Begin logged block
    sock._latency_optimization.state = H2O_SOCKET_LATENCY_OPTIMIZATION_STATE_NEEDS_UPDATE;
    LOG_VAR_INT(_latency_optimization.state); // Auto-logged
    }  // End logged block
    prepare_for_latency_optimized_write(&sock, &cond, 50000 /* rtt */, 1400 /* mss */, 14 /* cwnd_size */, 12 /* cwnd_avail */, 4,
                                        test_adjust_notsent_lowat);
    ok(sock._latency_optimization.state == H2O_SOCKET_LATENCY_OPTIMIZATION_STATE_DETERMINED);
    ok(sock._latency_optimization.suggested_tls_payload_size == 1400 - (5 + 8 + 16));
    ok(sock._latency_optimization.suggested_write_size == (1400 - (5 + 8 + 16)) * (14 - 3 + 1));
    ok(cb_ret_vars.minimize_notsent_lowat.call_cnt == 1);
    ok(cb_ret_vars.minimize_notsent_lowat.cur == 1);

    /* switches to B/W optimization when CWND becomes greater */
    {  // Begin logged block
    sock._latency_optimization.state = H2O_SOCKET_LATENCY_OPTIMIZATION_STATE_NEEDS_UPDATE;
    LOG_VAR_INT(_latency_optimization.state); // Auto-logged
    }  // End logged block
    prepare_for_latency_optimized_write(&sock, &cond, 50000 /* rtt */, 1400 /* mss */, (65535 / 1400) + 1 /* cwnd_size */,
                                        (65535 / 1400) + 1 /* cwnd_avail */, 4, test_adjust_notsent_lowat);
    ok(sock._latency_optimization.state == H2O_SOCKET_LATENCY_OPTIMIZATION_STATE_DETERMINED);
    ok(sock._latency_optimization.suggested_tls_payload_size == SIZE_MAX);
    ok(sock._latency_optimization.suggested_write_size == SIZE_MAX);
    ok(cb_ret_vars.minimize_notsent_lowat.call_cnt == 2);
    ok(cb_ret_vars.minimize_notsent_lowat.cur == 0);

    /* switches back to latency optimization when CWND becomes small */
    {  // Begin logged block
    sock._latency_optimization.state = H2O_SOCKET_LATENCY_OPTIMIZATION_STATE_NEEDS_UPDATE;
    LOG_VAR_INT(_latency_optimization.state); // Auto-logged
    }  // End logged block
    prepare_for_latency_optimized_write(&sock, &cond, 50000 /* rtt */, 1400 /* mss */, 8 /* cwnd_size */, 6 /* cwnd_avail */, 4,
                                        test_adjust_notsent_lowat);
    ok(sock._latency_optimization.state == H2O_SOCKET_LATENCY_OPTIMIZATION_STATE_DETERMINED);
    ok(sock._latency_optimization.suggested_tls_payload_size == 1400 - (5 + 8 + 16));
    ok(sock._latency_optimization.suggested_write_size == (1400 - (5 + 8 + 16)) * (8 - 3 + 1));
    ok(cb_ret_vars.minimize_notsent_lowat.call_cnt == 3);
    ok(cb_ret_vars.minimize_notsent_lowat.cur == 1);

    /* switches back to B/W optimization when loop time becomes greater than threshold */
    {  // Begin logged block
    sock._latency_optimization.state = H2O_SOCKET_LATENCY_OPTIMIZATION_STATE_NEEDS_UPDATE;
    LOG_VAR_INT(_latency_optimization.state); // Auto-logged
    }  // End logged block
    prepare_for_latency_optimized_write(&sock, &cond, 50000 /* rtt */, 1400 /* mss */, 8 /* cwnd_size */, 6 /* cwnd_avail */, 6,
                                        test_adjust_notsent_lowat);
    ok(sock._latency_optimization.state == H2O_SOCKET_LATENCY_OPTIMIZATION_STATE_DISABLED);
    ok(sock._latency_optimization.suggested_tls_payload_size == SIZE_MAX);
    ok(sock._latency_optimization.suggested_write_size == SIZE_MAX);
    ok(cb_ret_vars.minimize_notsent_lowat.call_cnt == 4);
    ok(cb_ret_vars.minimize_notsent_lowat.cur == 0);
}

static void test_zerocopy_buffers(void)
{
    struct st_h2o_socket_zerocopy_buffers_t buffers = {};

    /* push and release in-order */
    ok(zerocopy_buffers_is_empty(&buffers));
    for (uintptr_t i = 0; i < 100; ++i)
        zerocopy_buffers_push(&buffers, (void *)(i + 1));
    for (uintptr_t i = 0; i < 100; ++i) {
        ok(!zerocopy_buffers_is_empty(&buffers));
        ok(zerocopy_buffers_release(&buffers, i) == (void *)(i + 1));
    }
    ok(zerocopy_buffers_is_empty(&buffers));

    /* push 2, release 1 in-order */
    uintptr_t push = 100, release = 100;
    for (int i = 0; i < 100; ++i) {
        zerocopy_buffers_push(&buffers, (void *)push++);
        zerocopy_buffers_push(&buffers, (void *)push++);
        ok(zerocopy_buffers_release(&buffers, release) == (void *)release);
        ++release;
    }

    /* gaps */
    ok(zerocopy_buffers_release(&buffers, release + 1) == (void *)(release + 1));
    ok(zerocopy_buffers_release(&buffers, release + 2) == (void *)(release + 2));
    ok(buffers.first_counter == release);
    ok(zerocopy_buffers_release(&buffers, release) == (void *)release);
    ok(buffers.first_counter == release + 3);
    release += 3;

    while (!zerocopy_buffers_is_empty(&buffers)) {
        ok(zerocopy_buffers_release(&buffers, release) == (void *)release);
        ++release;
    }

    ok(push == release);

    zerocopy_buffers_dispose(&buffers);
}

static void test_zerocopy_buffers_random(void)
{
    struct st_h2o_socket_zerocopy_buffers_t buffers = {};

    int rseed = 13;
#define RAND() (rseed = (rseed * 1103515245 + 12345) & 255)

    /* push 1000 entries, repeating each of them at 25% probability */
    for (uintptr_t i = 0; i < 1000; ++i) {
        do {
            zerocopy_buffers_push(&buffers, (void *)(i + 100000));
        } while (RAND() >= 192);
    }

    uint8_t freed[1000] = {0};
    while (!zerocopy_buffers_is_empty(&buffers)) {
        uint64_t c = buffers.first_counter + RAND() % (buffers.last - buffers.first);
        if (buffers.bufs[c - buffers.first_counter + buffers.first] != NULL) {
            uintptr_t p = (uintptr_t)zerocopy_buffers_release(&buffers, c);
            if (p) {
                assert(!freed[p - 100000]);
                freed[p - 100000] = 1;
            }
        }
    }

    for (size_t i = 0; i < PTLS_ELEMENTSOF(freed); ++i)
        assert(freed[i]);

#undef RAND
}

void test_lib__common__socket_c(void)
{
    subtest("on_alpn_select", test_on_alpn_select);
    subtest("sliding_counter", test_sliding_counter);
    subtest("prepare_for_latency_optimization", test_prepare_for_latency_optimization);
    subtest("zerocopy_buffers", test_zerocopy_buffers);
    subtest("zerocopy_buffers_random", test_zerocopy_buffers_random);
}
