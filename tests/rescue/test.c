// Host tests for source/core/rescue.c: the request's mode, the report's
// format and parsing (round trip, any line order, Windows line ends,
// malformed values refused).
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "rescue.h"

static RescueMode mode_of(const char *s)
{
    return rescue_request_mode(s, strlen(s));
}

static void test_request_mode(void)
{
    assert(mode_of("") == RescueMode_Unlock);
    assert(rescue_request_mode(NULL, 0) == RescueMode_Unlock);
    assert(mode_of("unlock") == RescueMode_Unlock);
    assert(mode_of("please help") == RescueMode_Unlock);
    // A line that is the word alone, in any case, spaces around it ignored.
    assert(mode_of("delete") == RescueMode_Delete);
    assert(mode_of("DELETE\n") == RescueMode_Delete);
    assert(mode_of("DELETE\r\n") == RescueMode_Delete);
    assert(mode_of("  delete \r\n") == RescueMode_Delete);
    assert(mode_of("\tDelete\t") == RescueMode_Delete);
    assert(mode_of("unlock\ndelete") == RescueMode_Delete);
    assert(mode_of("please\r\n\r\n delete\r\nthanks\r\n") == RescueMode_Delete);
    assert(mode_of("\xEF\xBB\xBF" "delete") == RescueMode_Delete);   // Notepad's byte-order mark
    assert(mode_of("\xEF\xBB\xBF" "Delete\r\n") == RescueMode_Delete);
    // The word inside other text is not enough.
    assert(mode_of("don't delete anything") == RescueMode_Unlock);
    assert(mode_of("undelete") == RescueMode_Unlock);
    assert(mode_of("delete everything") == RescueMode_Unlock);
    assert(mode_of("  delete everything") == RescueMode_Unlock);
    assert(mode_of("deleted") == RescueMode_Unlock);
    assert(mode_of("delet") == RescueMode_Unlock);
    assert(mode_of("del ete") == RescueMode_Unlock);
    assert(mode_of("\n\n") == RescueMode_Unlock);
    // A byte-order mark only counts at the very start.
    assert(mode_of("x\n\xEF\xBB\xBF" "delete") == RescueMode_Unlock);
    // Only the first `len` bytes count.
    assert(rescue_request_mode("delete", 5) == RescueMode_Unlock);
    assert(rescue_request_mode("deletex", 6) == RescueMode_Delete);
    assert(rescue_request_mode("\xEF\xBB", 2) == RescueMode_Unlock);
}

static void test_round_trip(void)
{
    const RescueReport all[] = {
        { RescueMode_Unlock, RescueResult_Ok, 0, 1, RescueRequest_Removed },
        { RescueMode_Unlock, RescueResult_NoPin, 0, 0, RescueRequest_Renamed },
        { RescueMode_Unlock, RescueResult_Failed, 0xC8A0F, 0, RescueRequest_Kept },
        { RescueMode_Delete, RescueResult_Ok, 0, 0, RescueRequest_Renamed },
        { RescueMode_Delete, RescueResult_Failed, 0xFFFFFFFFu, 4294967295u, RescueRequest_Removed },
        { RescueMode_Delete, RescueResult_Refused, 0, 0, RescueRequest_Kept },
    };
    for (size_t i = 0; i < sizeof(all) / sizeof(all[0]); i++) {
        char buf[128];
        const size_t n = rescue_report_format(&all[i], buf, sizeof(buf));
        assert(n > 0 && n == strlen(buf));
        RescueReport back;
        memset(&back, 0x5A, sizeof(back));
        assert(rescue_report_parse(buf, n, &back));
        assert(back.mode == all[i].mode && back.result == all[i].result);
        assert(back.rc == all[i].rc && back.unlocks == all[i].unlocks);
        assert(back.request == all[i].request);
    }

    char buf[128];
    const RescueReport r = { RescueMode_Unlock, RescueResult_Ok, 0, 2, RescueRequest_Removed };
    rescue_report_format(&r, buf, sizeof(buf));
    assert(strcmp(buf, "mode=unlock\nresult=ok\nrc=0x00000000\nunlocks=2\nrequest=removed\n") == 0);
}

static void test_format_too_small(void)
{
    const RescueReport r = { RescueMode_Unlock, RescueResult_Ok, 0, 0, RescueRequest_Removed };
    char buf[10];
    memset(buf, 'x', sizeof(buf));
    assert(rescue_report_format(&r, buf, sizeof(buf)) == 0 && buf[0] == '\0');
    assert(rescue_report_format(&r, buf, 0) == 0);
    assert(rescue_report_format(NULL, buf, sizeof(buf)) == 0 && buf[0] == '\0');
}

static bool parses(const char *text, RescueReport *out)
{
    return rescue_report_parse(text, strlen(text), out);
}

static void test_parse(void)
{
    RescueReport r;
    // Any order, CR LF, unknown keys, blank lines, no trailing newline.
    assert(parses("unlocks=3\r\nrc=12\r\n\r\nresult=failed\r\nfuture=1\r\nmode=delete", &r));
    assert(r.mode == RescueMode_Delete && r.result == RescueResult_Failed && r.rc == 12 && r.unlocks == 3);
    // rc, unlocks and request are optional (a report from before request=).
    assert(parses("mode=unlock\nresult=no_pin\n", &r));
    assert(r.result == RescueResult_NoPin && r.rc == 0 && r.unlocks == 0 && r.request == RescueRequest_Removed);
    assert(parses("mode=delete\nresult=refused\nrequest=kept\n", &r));
    assert(r.result == RescueResult_Refused && r.request == RescueRequest_Kept);

    // Refused, and *out left as it was.
    const char *bad[] = {
        "",
        "result=ok\n",                              // no mode
        "mode=unlock\n",                            // no result
        "mode=wipe\nresult=ok\n",
        "mode=unlock\nresult=maybe\n",
        "mode=unlock\nresult=ok\nrc=0xZZ\n",
        "mode=unlock\nresult=ok\nrc=0x1FFFFFFFF\n",  // over 32 bits
        "mode=unlock\nresult=ok\nunlocks=-1\n",
        "mode=unlock\nresult=ok\nunlocks=\n",
        "mode=Unlock\nresult=ok\n",                 // written by the sysmodule: exact
        "mode= unlock\nresult=ok\n",
        "mode=unlock\nresult=ok\nrequest=gone\n",
        "mode=unlock\nresult=ok\nrequest=\n",
    };
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        memset(&r, 0x5A, sizeof(r));
        RescueReport before = r;
        assert(!parses(bad[i], &r));
        assert(memcmp(&r, &before, sizeof(r)) == 0);
    }
    assert(!rescue_report_parse(NULL, 0, &r));
    assert(!rescue_report_parse("mode=unlock\nresult=ok\n", 22, NULL));
    // Only `len` bytes are read.
    assert(!rescue_report_parse("mode=unlock\nresult=ok\n", 12, &r));
}

static void test_names(void)
{
    assert(strcmp(rescue_mode_name(RescueMode_Unlock), "unlock") == 0);
    assert(strcmp(rescue_mode_name(RescueMode_Delete), "delete") == 0);
    assert(strcmp(rescue_result_name(RescueResult_Ok), "ok") == 0);
    assert(strcmp(rescue_result_name(RescueResult_NoPin), "no_pin") == 0);
    assert(strcmp(rescue_result_name(RescueResult_Failed), "failed") == 0);
    assert(strcmp(rescue_result_name(RescueResult_Refused), "refused") == 0);
    assert(strcmp(rescue_request_name(RescueRequest_Removed), "removed") == 0);
    assert(strcmp(rescue_request_name(RescueRequest_Renamed), "renamed") == 0);
    assert(strcmp(rescue_request_name(RescueRequest_Kept), "kept") == 0);
}

int main(void)
{
    test_request_mode();
    test_round_trip();
    test_format_too_small();
    test_parse();
    test_names();
    puts("rescue request and report assertions passed");
    return 0;
}
