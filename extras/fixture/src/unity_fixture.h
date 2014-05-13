//- Copyright (c) 2010 James Grenning and Contributed to Unity Project
/* ==========================================
    Unity Project - A Test Framework for C
    Copyright (c) 2007 Mike Karlesky, Mark VanderVoord, Greg Williams
    [Released under MIT License. Please refer to license.txt for details]
========================================== */

#ifndef UNITY_FIXTURE_H_
#define UNITY_FIXTURE_H_

#include "unity.h"
#include "unity_internals.h"
/* DX_PATCH: Removing malloc overrides - it cannot be used with stdlib.h included
   before or after, as documented in https://github.com/ThrowTheSwitch/Unity/issues/24,
   which is a too severe limitation for practical use. */
#if defined(UNITY_DYNAMIC_MEM_DEBUG)
#include "unity_fixture_malloc_overrides.h"
#endif /* UNITY_DYNAMIC_MEM_DEBUG */

#include "unity_fixture_internals.h"

int UnityMain(int argc, char* argv[], void (*runAllTests)(struct _Unity * const unity_p), struct _Unity * const unity_p);


#define TEST_GROUP(group)\
    static const char* TEST_GROUP_##group = #group; \
    typedef struct Test_##group##_Data_ Test_##group##_Data;

#define TEST_GROUP_DATA_TYPE(group) \
    Test_##group##_Data

#define TEST_GROUP_DATA_START(group)\
    typedef struct Test_##group##_Data_ {

#define TEST_GROUP_DATA_END(group)\
    }Test_##group##_Data;

#define TEST_GROUP_DATA_CREATE(group)\
    Test_##group##_Data *_td = malloc(sizeof(*_td));\
    DX_BZERO(*_td); \
    TEST_ASSERT_NOT_NULL(_td); \
    unity_p->testLocalStorage = _td;

#define TEST_GROUP_DATA_DESTROY(group)\
    free(unity_p->testLocalStorage);\
    unity_p->testLocalStorage = NULL;

#define TEST_SETUP(group) void TEST_##group##_SETUP(struct _Unity * const unity_p);\
    void TEST_##group##_SETUP(struct _Unity * const unity_p)

#define TEST_TEAR_DOWN(group) void TEST_##group##_TEAR_DOWN(Test_##group##_Data * const _td, struct _Unity * const unity_p);\
    void TEST_##group##_TEAR_DOWN(Test_##group##_Data * const _td, struct _Unity * const unity_p)


#define TEST(group, name) \
    void TEST_##group##_##name##_(Test_##group##_Data * const _td, struct _Unity * const unity_p);\
    void TEST_##group##_##name##_run(struct _Unity * const unity_p);\
    void TEST_##group##_##name##_run(struct _Unity * const unity_p)\
    {\
        unity_p->testLocalStorage = NULL; \
        UnityTestRunner(TEST_##group##_SETUP,\
            (unityTestfunction *)TEST_##group##_##name##_,\
            (unityTestfunction *)TEST_##group##_TEAR_DOWN,\
            "TEST(" #group ", " #name ")",\
            TEST_GROUP_##group, #name,\
            __FILE__, __LINE__, unity_p);\
    }\
    void TEST_##group##_##name##_(Test_##group##_Data * const _td, struct _Unity * const unity_p)

#define IGNORE_TEST(group, name) \
    void TEST_##group##_##name##_(Test_##group##_Data * const _td, struct _Unity * const unity_p);\
    void TEST_##group##_##name##_run(struct _Unity * const unity_p)\
    {\
        UnityIgnoreTest("IGNORE_TEST(" #group ", " #name ")", unity_p);\
    }\
    void TEST_##group##_##name##_(Test_##group##_Data * const _td, struct _Unity * const unity_p)

#define DECLARE_TEST_CASE(group, name) \
    void TEST_##group##_##name##_run(struct _Unity * const unity_p)

#define RUN_TEST_CASE(group, name) \
    { DECLARE_TEST_CASE(group, name);\
      TEST_##group##_##name##_run(unity_p); }

//This goes at the bottom of each test file or in a separate c file
#define TEST_GROUP_RUNNER(group)\
    void TEST_##group##_GROUP_RUNNER_runAll(struct _Unity * const unity_p);\
    void TEST_##group##_GROUP_RUNNER(struct _Unity * const unity_p);\
    void TEST_##group##_GROUP_RUNNER(struct _Unity * const unity_p)\
    {\
        TEST_##group##_GROUP_RUNNER_runAll(unity_p);\
    }\
    void TEST_##group##_GROUP_RUNNER_runAll(struct _Unity * const unity_p)

//Call this from main
#define RUN_TEST_GROUP(group)\
    { void TEST_##group##_GROUP_RUNNER(struct _Unity * const unity_p);\
      TEST_##group##_GROUP_RUNNER(unity_p); }

//CppUTest Compatibility Macros
#if defined(UNITY_CPP_UNIT_COMPAT)
#define UT_PTR_SET(ptr, newPointerValue)               UnityPointer_Set((void**)&ptr, (void*)newPointerValue, unity_p)
#define TEST_ASSERT_POINTERS_EQUAL(expected, actual)   TEST_ASSERT_EQUAL_PTR(expected, actual)
#define TEST_ASSERT_BYTES_EQUAL(expected, actual)      TEST_ASSERT_EQUAL_HEX8(0xff & (expected), 0xff & (actual))
#define FAIL(message)                                  TEST_FAIL((message))
#define CHECK(condition)                               TEST_ASSERT_TRUE((condition))
#define LONGS_EQUAL(expected, actual)                  TEST_ASSERT_EQUAL_INT((expected), (actual))
#define STRCMP_EQUAL(expected, actual)                 TEST_ASSERT_EQUAL_STRING((expected), (actual))
#define DOUBLES_EQUAL(expected, actual, delta)         TEST_ASSERT_FLOAT_WITHIN(((expected), (actual), (delta))
#endif /*UNITY_CPP_UNIT_COMPAT */

#if defined(UNITY_DYNAMIC_MEM_DEBUG)
void UnityMalloc_MakeMallocFailAfterCount(int count);

/* DX_PATCH: Pre-declarations required to avoid warnings */
void * unity_malloc(size_t size);
void unity_free(void * mem);
void* unity_calloc(size_t num, size_t size);
void* unity_realloc(void * oldMem, size_t size);

#endif // UNITY_DYNAMIC_MEM_DEBUG

#endif /* UNITY_FIXTURE_H_ */
