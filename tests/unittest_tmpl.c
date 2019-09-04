/**
 * This is the unit test template.
 *
 * All unit tests will be wrapped into this file.
 *
 * In the respective unit test source file, each test is
 * marked with @Test.
 */
#include <CUnit/Basic.h>

/***************************************************************************/

int init_suite1(void)
{
	return 0;
}

int clean_suite1(void)
{
	return 0;
}

/***************************************************************************/

struct unit_test
{
	char *name;
	void (*test)(void);
};

extern struct unit_test unit_tests[];

#ifdef UNITTEST_FILE
#include UNITTEST_FILE
#endif

#ifdef UNITTEST_TABLE
#include UNITTEST_TABLE
#else
struct unit_test unit_tests[1];
#endif

/***************************************************************************/

int main(int argc, char *argv[])
{
	const char *single_test;
	int i;

	single_test = NULL;

	CU_pSuite pSuite = NULL;

	if (argc >= 2)
	{
		int j;

		for (j = 1; j < argc; j++)
		{
			if (*argv[j] == '-')
			{
				if (!strcmp(argv[j], "--list-tests"))
				{
					for (i=0;unit_tests[i].test;i++)
					{
						printf("%s\n", unit_tests[i].name);
					}

					return 0;
				}

				if (!strcmp(argv[j], "-h") || !strcmp(argv[j], "--help"))
				{
					fprintf(stderr, "Usage: %s [testname] [--list-tests] [--help]\n", argv[0]);
					return 0;
				}

				fprintf(stderr, "Option \'%s\' is unknown\n", argv[j]);
				return -1;
			}

			single_test = argv[j];
		}
	}

	/* initialize the CUnit test registry */
	if (CUE_SUCCESS != CU_initialize_registry())
		return CU_get_error();

	/* add a suite to the registry */
	if (!(pSuite = CU_add_suite("Test Suite", init_suite1, clean_suite1)))
	{
		CU_cleanup_registry();
		return CU_get_error();
	}

	if (single_test)
	{
		int present = 0;

		/* Check the presence of the single test */
		for (i=0;unit_tests[i].test;i++)
		{
			if (!strcmp(unit_tests[i].name, single_test))
			{
				present = 1;
				break;
			}
		}

		if (!present)
		{
			fprintf(stderr, "Test \'%s\' is not known\n", single_test);
			return -1;
		}

		if (!CU_add_test(pSuite, unit_tests[i].name, unit_tests[i].test))
		{
			CU_cleanup_registry();
			return CU_get_error();
		}
	} else
	{
		/* Add all tests */
		for (i=0;unit_tests[i].test;i++)
		{
			if (!CU_add_test(pSuite, unit_tests[i].name, unit_tests[i].test))
			{
				CU_cleanup_registry();
				return CU_get_error();
			}
		}
	}

	/* Run all tests using the CUnit Basic interface */
	CU_basic_set_mode(CU_BRM_VERBOSE);
	CU_basic_run_tests();
	CU_cleanup_registry();

	return CU_get_error();
}

