//- Copyright (c) 2010 James Grenning and Contributed to Unity Project
/* ==========================================
    Unity Project - A Test Framework for C
    Copyright (c) 2007 Mike Karlesky, Mark VanderVoord, Greg Williams
    [Released under MIT License. Please refer to license.txt for details]
========================================== */

#include <string.h>
#include <assert.h>
#include <stdlib.h>

#include "unity_fixture.h"
#include "unity_internals.h"

//If you decide to use the function pointer approach.
/* DX_PATCH: As outputChar pointer approach is not used, and this generates a warning for
   MSVC - removed. */
/* int(*outputChar)(int) = putchar; */

/* DX_PATCH: Fix GCC warning "no previous prototype for..." */
void setUp(struct _Unity * const unity_p);
void tearDown(struct _Unity * const unity_p);
void setUp(struct _Unity * const unity_p)    { /*does nothing*/ }
void tearDown(struct _Unity * const unity_p) { /*does nothing*/ }

/* DX_PATCH: "static" modifier required to avoid GCC warning: no previous prototype for... */
static void announceTestRun(unsigned int runNumber, struct _Unity * const unity_p)
{
    UnityPrint("Unity test run ", unity_p);
    UnityPrintNumber(runNumber+1, unity_p);
    UnityPrint(" of ", unity_p);
    UnityPrintNumber(unity_p->RepeatCount, unity_p);
    UNITY_OUTPUT_CHAR('\n');
}

int UnityMain(int argc, char* argv[], void (*runAllTests)(struct _Unity * const unity_p), struct _Unity * const unity_p)
{
    int result = UnityGetCommandLineOptions(argc, argv, unity_p);
    unsigned int r;
    if (result != 0)
        return result;

    for (r = 0; r < unity_p->RepeatCount; r++)
    {
        announceTestRun(r, unity_p);
        UnityBegin(unity_p);
        runAllTests(unity_p);
        UNITY_OUTPUT_CHAR('\n');
        UnityEnd(unity_p);
    }

    return UnityFailureCount(unity_p);
}

static int selected(const char * filter, const char * name)
{
    if (filter == 0)
        return 1;
    return strstr(name, filter) ? 1 : 0;
}

static int testSelected(const char* test, struct _Unity * const unity_p)
{
    return selected(unity_p->NameFilter, test);
}

static int groupSelected(const char* group, struct _Unity * const unity_p)
{
    return selected(unity_p->GroupFilter, group);
}

static void runTestCase(struct _Unity * const unity_p)
{

}

void UnityTestRunner(unityfunction * setup,
        unityTestfunction * body,
        unityTestfunction * teardown,
        const char * printableName,
        const char * group,
        const char * name,
        const char * file, int line, struct _Unity * const unity_p)
{
    if (testSelected(name, unity_p) && groupSelected(group, unity_p))
    {
        unity_p->CurrentTestFailed = 0;
        unity_p->TestFile = file;
        unity_p->CurrentTestName = printableName;
        unity_p->CurrentTestLineNumber = line;
        if (!unity_p->Verbose)
            UNITY_OUTPUT_CHAR('.');
        else
            UnityPrint(printableName, unity_p);

        unity_p->NumberOfTests++;
#if defined(UNITY_DYNAMIC_MEM_DEBUG)
        UnityMalloc_StartTest();
#endif
#if defined(UNITY_CPP_UNIT_COMPAT)
        UnityPointer_Init(unity_p);
#endif 
        runTestCase(unity_p);
        /* remember setup has failed - skip teardown if so*/
        bool hasSetupFailed = false;
        if (TEST_PROTECT())
        {
            setup(unity_p);
            /*DX_PATCH for jumpless version. If setup failed don't perform the test*/
            if (!unity_p->CurrentTestFailed) 
            {
                body(unity_p->testLocalStorage, unity_p);
            }
            else
            {
                hasSetupFailed  = true;
            }
        }
        if (TEST_PROTECT() && !hasSetupFailed )
        {
            teardown(unity_p->testLocalStorage, unity_p);
        }
        if (TEST_PROTECT())
        {
#if defined(UNITY_CPP_UNIT_COMPAT)
            UnityPointer_UndoAllSets(unity_p);
#endif
#if defined(UNITY_DYNAMIC_MEM_DEBUG)
            if (!unity_p->CurrentTestFailed)
                UnityMalloc_EndTest(unity_p);
#endif
        }
        UnityConcludeFixtureTest(unity_p);
    }
}

void UnityIgnoreTest(const char * printableName, struct _Unity * const unity_p)
{
    unity_p->NumberOfTests++;
    unity_p->CurrentTestIgnored = 1;
    if (!unity_p->Verbose)
        UNITY_OUTPUT_CHAR('!');
    else
        UnityPrint(printableName, unity_p);
    UnityConcludeFixtureTest(unity_p);
}

#if defined(UNITY_DYNAMIC_MEM_DEBUG)
//-------------------------------------------------
//Malloc and free stuff
//
#define MALLOC_DONT_FAIL -1
static int malloc_count;
static int malloc_fail_countdown = MALLOC_DONT_FAIL;

void UnityMalloc_StartTest(void)
{
    malloc_count = 0;
    malloc_fail_countdown = MALLOC_DONT_FAIL;
}

void UnityMalloc_EndTest( struct _Unity * const unity_p )
{
    malloc_fail_countdown = MALLOC_DONT_FAIL;
    if (malloc_count != 0)
    {
        TEST_FAIL_MESSAGE("This test leaks!");
    }
}

void UnityMalloc_MakeMallocFailAfterCount(int countdown)
{
    malloc_fail_countdown = countdown;
}

#ifdef malloc
#undef malloc
#endif

#ifdef free
#undef free
#endif

#include <stdlib.h>
#include <string.h>

typedef struct GuardBytes
{
    size_t size;
    char guard[sizeof(size_t)];
} Guard;


static const char * end = "END";

void * unity_malloc(size_t size)
{
    char* mem;
    Guard* guard;

    if (malloc_fail_countdown != MALLOC_DONT_FAIL)
    {
        if (malloc_fail_countdown == 0)
            return 0;
        malloc_fail_countdown--;
    }

    malloc_count++;

    guard = (Guard*)malloc(size + sizeof(Guard) + 4);
    assert(guard);
    guard->size = size;
    mem = (char*)&(guard[1]);
    memcpy(&mem[size], end, strlen(end) + 1);

    return (void*)mem;
}

static int isOverrun(void * mem)
{
    Guard* guard = (Guard*)mem;
    char* memAsChar = (char*)mem;
    guard--;

    return strcmp(&memAsChar[guard->size], end) != 0;
}

static void release_memory(void * mem)
{
    Guard* guard = (Guard*)mem;
    guard--;

    malloc_count--;
    free(guard);
}

void unity_free(void * mem)
{
    int overrun = isOverrun(mem);//strcmp(&memAsChar[guard->size], end) != 0;
    release_memory(mem);
    if (overrun)
    {
        TEST_FAIL_MESSAGE("Buffer overrun detected during free()");
    }
}

void* unity_calloc(size_t num, size_t size)
{
    void* mem = unity_malloc(num * size);
    memset(mem, 0, num*size);
    return mem;
}

void* unity_realloc(void * oldMem, size_t size)
{
    Guard* guard = (Guard*)oldMem;
//    char* memAsChar = (char*)oldMem;
    void* newMem;

    if (oldMem == 0)
        return unity_malloc(size);

    guard--;
    if (isOverrun(oldMem))
    {
        release_memory(oldMem);
        /*DX_PATCH for jumless version*/
        UnityPrint("Buffer overrun detected during realloc()", unity_p);
        return 0;
    }

    if (size == 0)
    {
        release_memory(oldMem);
        return 0;
    }

    if (guard->size >= size)
        return oldMem;

    newMem = unity_malloc(size);
    memcpy(newMem, oldMem, guard->size);
    unity_free(oldMem);
    return newMem;
}

#endif /* UNITY_DYNAMIC_MEM_DEBUG */

#if defined(UNITY_CPP_UNIT_COMPAT)

//--------------------------------------------------------
//Automatic pointer restoration functions
typedef struct _PointerPair
{
    struct _PointerPair * next;
    void ** pointer;
    void * old_value;
} PointerPair;

enum {MAX_POINTERS=50};
static PointerPair pointer_store[MAX_POINTERS];
static int pointer_index = 0;

void UnityPointer_Init( struct _Unity * const unity_p )
{
    pointer_index = 0;
}

void UnityPointer_Set(void ** ptr, void * newValue, struct _Unity * const unity_p)
{
    if (pointer_index >= MAX_POINTERS)
        TEST_FAIL_MESSAGE("Too many pointers set");

    pointer_store[pointer_index].pointer = ptr;
    pointer_store[pointer_index].old_value = *ptr;
    *ptr = newValue;
    pointer_index++;
}

void UnityPointer_UndoAllSets( struct _Unity * const unity_p )
{
    while (pointer_index > 0)
    {
        pointer_index--;
        *(pointer_store[pointer_index].pointer) =
        pointer_store[pointer_index].old_value;

    }
}

#endif
int UnityFailureCount( struct _Unity * const unity_p )
{
    return unity_p->TestFailures;
}

int UnityGetCommandLineOptions(int argc, char* argv[], struct _Unity * const unity_p)
{
    int i;
    unity_p->Verbose = 0;
    unity_p->GroupFilter = 0;
    unity_p->NameFilter = 0;
    unity_p->RepeatCount = 1;

    if (argc == 1)
        return 0;

    for (i = 1; i < argc; )
    {
        if (strcmp(argv[i], "-v") == 0)
        {
            unity_p->Verbose = 1;
            i++;
        }
        else if (strcmp(argv[i], "-g") == 0)
        {
            i++;
            if (i >= argc)
                return 1;
            unity_p->GroupFilter = argv[i];
            i++;
        }
        else if (strcmp(argv[i], "-n") == 0)
        {
            i++;
            if (i >= argc)
                return 1;
            unity_p->NameFilter = argv[i];
            i++;
        }
        else if (strcmp(argv[i], "-r") == 0)
        {
            unity_p->RepeatCount = 2;
            i++;
            if (i < argc)
            {
                if (*(argv[i]) >= '0' && *(argv[i]) <= '9')
                {
                    unity_p->RepeatCount = atoi(argv[i]);
                    i++;
                }
            }
        } else {
            // ignore unknown parameter
            i++;
        }
    }
    return 0;
}

void UnityConcludeFixtureTest( struct _Unity * const unity_p )
{
    if (unity_p->CurrentTestIgnored)
    {
        if (unity_p->Verbose)
        {
            UNITY_OUTPUT_CHAR('\n');
        }
        unity_p->TestIgnores++;
    }
    else if (!unity_p->CurrentTestFailed)
    {
        if (unity_p->Verbose)
        {
            UnityPrint(" PASS", unity_p);
            UNITY_OUTPUT_CHAR('\n');
        }
    }
    else if (unity_p->CurrentTestFailed)
    {
        unity_p->TestFailures++;
    }

    unity_p->CurrentTestFailed = 0;
    unity_p->CurrentTestIgnored = 0;
}
