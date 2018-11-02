#include "../src/utilities.h"
#include <CUnit/Basic.h>
#include "../src/min_queue.h"

int get_port(const char *portstring, const char *caller) {
	long l = -1;
	if ((l = strtol(portstring, NULL, 10)) <= 0) {
		fprintf(stderr, "[ERROR] [%s] Invalid port: %s\n", caller, portstring);
		return -1;
	} else if (l < 1024) {
		fprintf(stderr, "[ERROR] [%s] Reserved port (value cannot be less than 1024): %s\n", caller, portstring);
		return -1;
	} else if (l > 65535) {
		fprintf(stderr, "[ERROR] [%s] Invalid port (value must be less than or equal to 65535): %s\n", caller, portstring);
		return -1;
	}
	fprintf(stderr, "[LOG] [%s] Port initialised\n", caller);
	return (int) l;
}

int suc(int seqnum) {
	return (seqnum + 1) % 256;
}

int compare(const uint8_t seqa, const uint8_t seqb) {
	if (seqa >= seqb && seqa - seqb > 200) {
		return 0;
	} else if (seqa < seqb && seqb - seqa > 200) {
		return 1;
	} else if (seqa > seqb) {
		return 1;
	} else {
		return 0;
	}
}

/**
 * Tests whether ports are parsed correctly in the regular case
 */
void get_port_regular(void)
{
	const char *portstring = "6969";
	int port = get_port(portstring, "TEST");
	int true_port = 6969;
	CU_ASSERT_EQUAL(port, true_port);
}

/**
 * Tests whether ports are parsed correctly in the system port case
 */
void get_port_system(void)
{
	const char *portstring = "420";
	int port = get_port(portstring, "TEST");
	int true_port = -1;
	CU_ASSERT_EQUAL(port, true_port);
}

/**
 * Tests whether ports are parsed correctly in the case where the port number is too big
 */
void get_port_big(void)
{
	const char *portstring = "31415926";
	int port = get_port(portstring, "TEST");
	int true_port = -1;
	CU_ASSERT_EQUAL(port, true_port);
}

/**
 * Tests whether ports are parsed correctly in the case where the port number is negative
 */
void get_port_negative(void)
{
	const char *portstring = "-6969";
	int port = get_port(portstring, "TEST");
	int true_port = -1;
	CU_ASSERT_EQUAL(port, true_port);
}

/**
 * Tests whether ports are parsed correctly in the case where the port number is 0
 */
void get_port_zero(void)
{
	const char *portstring = "0";
	int port = get_port(portstring, "TEST");
	int true_port = -1;
	CU_ASSERT_EQUAL(port, true_port);
}

/**
 * Tests whether successive sequence numbers are correctly calculated in the regular case
 */
 void succ_regular(void)
 {
 	int seqnum = 69;
 	int succ = suc(seqnum);
 	int true_succ = 70;
 	CU_ASSERT_EQUAL(true_succ, succ);
 }

/**
 * Tests whether successive sequence numbers are correctly calculated in the wrapping case
 */
void succ_255(void)
{
	int seqnum = 255;
	int succ = suc(seqnum);
	int true_succ = 0;
	CU_ASSERT_EQUAL(true_succ, succ);
}

/**
 * Tests whether successive sequence numbers are correctly calculated in the wrapping case
 */
void succ_0(void)
{
	int seqnum = 0;
	int succ = suc(seqnum);
	int true_succ = 1;
	CU_ASSERT_EQUAL(true_succ, succ);
}

/**
 * Tests whether successive sequence numbers are correctly calculated for negative numbers with wrapping
 */
void succ_negative(void)
{
	int seqnum = -1;
	int succ = suc(seqnum);
	int true_succ = 0;
	CU_ASSERT_EQUAL(true_succ, succ);
}

/**
 * Tests whether sequence numbers are correctly compared in the min_queue in the regular case
 */
void cmp_reg(void)
{
	uint8_t a = 34;
	uint8_t b = 45;
	int cmp = compare(a, b);
	CU_ASSERT_EQUAL(cmp, 0);
}

/**
 * Tests whether sequence numbers are correctly compared in the min_queue in the wrapping case
 */
void cmp_wrap(void)
{
	uint8_t a = 0;
	uint8_t b = 255;
	int cmp = compare(a, b);
	CU_ASSERT_EQUAL(cmp, 1);
}

int main(int argc, const char *argv[]) {
	argc = argc;
	argv = argv;
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
