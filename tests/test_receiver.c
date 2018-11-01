#include "../src/utilities.h"
#include <CUnit/Basic.h>
#include "../src/min_queue.h"

/**
 * Tests whether ports are parsed correctly in the regular case
 */
void get_port_regular(void)
{
	const char *port = "6969";
	int port = get_port(port, "TEST");
	int true_port = 6969;
	CU_ASSERT_EQUAL(port, true_port);
}

/**
 * Tests whether ports are parsed correctly in the system port case
 */
void get_port_system(void)
{
	const char *port = "420";
	int port = get_port(port, "TEST");
	int true_port = -1;
	CU_ASSERT_EQUAL(port, true_port);
}

/**
 * Tests whether ports are parsed correctly in the case where the port number is too big
 */
void get_port_big(void)
{
	const char *port = "31415926";
	int port = get_port(port, "TEST");
	int true_port = -1;
	CU_ASSERT_EQUAL(port, true_port);
}

/**
 * Tests whether ports are parsed correctly in the case where the port number is negative
 */
void get_port_negative(void)
{
	const char *port = "-6969";
	int port = get_port(port, "TEST");
	int true_port = -1;
	CU_ASSERT_EQUAL(port, true_port);
}

/**
 * Tests whether ports are parsed correctly in the case where the port number is 0
 */
void get_port_zero(void)
{
	const char *port = "0";
	int port = get_port(port, "TEST");
	int true_port = -1;
	CU_ASSERT_EQUAL(port, true_port);
}

/**
 * Tests whether successive sequence numbers are correctly calculated in the regular case
 */
 void succ_regular(void)
 {
 	int seqnum = 69;
 	int succ = succ(seqnum);
 	int true_port = 70;
 	CU_ASSERT_EQUAL(true_port, succ);
 }

/**
 * Tests whether successive sequence numbers are correctly calculated in the wrapping case
 */
void succ_255(void)
{
	int seqnum = 255;
	int succ = succ(seqnum);
	int true_port = 0;
	CU_ASSERT_EQUAL(true_port, succ);
}

/**
 * Tests whether successive sequence numbers are correctly calculated in the wrapping case
 */
void succ_0(void)
{
	int seqnum = 0;
	int succ = succ(seqnum);
	int true_port = -1;
	CU_ASSERT_EQUAL(true_port, succ);
}

/**
 * Tests whether successive sequence numbers are correctly calculated for negative numbers with wrapping
 */
void succ_negative(void)
{
	int seqnum = -1;
	int succ = succ(seqnum);
	int true_port = 0;
	CU_ASSERT_EQUAL(true_port, succ);
}

/**
 * Tests whether sequence numbers are correctly compared in the min_queue in the regular case
 */
void cmp_reg(void)
{
	pkt_t a = pkt_new();
	pkt_set_seqnum(a, 34);
	pkt_t b = pkt_new();
	pkt_set_seqnum(b, 45);
	int cmp = cmp(a, b);
	CU_ASSERT_EQUAL(cmp, 0);
}

/**
 * Tests whether sequence numbers are correctly compared in the min_queue in the wrapping case
 */
void cmp_wrap(void)
{
	pkt_t a = pkt_new();
	pkt_set_seqnum(a, 0);
	pkt_t b = pkt_new();
	pkt_set_seqnum(b, 255);
	int cmp = cmp(a, b);
	CU_ASSERT_EQUAL(cmp, 1);
}

int main(int argc, const char *argv[]) {
	CU_pSuite pSuite = NULL;
	// Initialise Suite.
	if (CUE_SUCCESS != CU_initialize_registry())
	{
		return CU_get_error();
	}

	// Create Suite.
	pSuite = CU_add_suite("Suite", NULL, NULL);
	if (NULL == pSuite)
	{
		CU_cleanup_registry();
		return CU_get_error();
	}

	// Add to Suite.
	if (NULL == CU_add_test(pSuite, "get_port_regular", get_port_regular) ||
	NULL == CU_add_test(pSuite, "get_port_system", get_port_system) ||
	NULL == CU_add_test(pSuite, "get_port_big", get_port_big) ||
	NULL == CU_add_test(pSuite, "get_port_negative", get_port_negative) ||
	NULL == CU_add_test(pSuite, "get_port_zero", get_port_zero) ||
	NULL == CU_add_test(pSuite, "succ_regular", succ_regular) ||
	NULL == CU_add_test(pSuite, "succ_255", succ_255) ||
	NULL == CU_add_test(pSuite, "succ_0", succ_0) ||
	NULL == CU_add_test(pSuite, "succ_negative", succ_negative) ||
	NULL == CU_add_test(pSuite, "cmp_reg", cmp_reg) ||
	NULL == CU_add_test(pSuite, "cmp_wrap", cmp_wrap))
    {
		CU_cleanup_registry();
		return CU_get_error();
	}

	// Launch tests.
	CU_basic_set_mode(CU_BRM_VERBOSE);
	CU_basic_run_tests();
	CU_cleanup_registry();

	return CU_get_error();
}
