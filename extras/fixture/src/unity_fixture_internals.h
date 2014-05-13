//- Copyright (c) 2010 James Grenning and Contributed to Unity Project
/* ==========================================
    Unity Project - A Test Framework for C
    Copyright (c) 2007 Mike Karlesky, Mark VanderVoord, Greg Williams
    [Released under MIT License. Please refer to license.txt for details]
========================================== */

#ifndef UNITY_FIXTURE_INTERNALS_H_
#define UNITY_FIXTURE_INTERNALS_H_

typedef void unityfunction(struct _Unity * const unity_p);
typedef void unityTestfunction(void * _td, struct _Unity * const unity_p);

void UnityTestRunner(unityfunction * setup,
        unityTestfunction * body,
        unityTestfunction * teardown,
        const char * printableName,
        const char * group,
        const char * name,
        const char * file, int line, struct _Unity * const unity_p);

void UnityIgnoreTest(const char * printableName, struct _Unity * const unity_p);
void UnityMalloc_StartTest(void);
void UnityMalloc_EndTest(struct _Unity * const unity_p);
int UnityFailureCount(struct _Unity * const unity_p);
int UnityGetCommandLineOptions(int argc, char* argv[], struct _Unity * const unity_p);
void UnityConcludeFixtureTest(struct _Unity * const unity_p);

void UnityPointer_Set(void ** ptr, void * newValue, struct _Unity * const unity_p);
void UnityPointer_UndoAllSets(struct _Unity * const unity_p);
void UnityPointer_Init(struct _Unity * const unity_p);

void UnityAssertEqualPointer(const void * expected,
                            const void * actual,
                            const char* msg,
                            const UNITY_LINE_TYPE lineNumber, struct _Unity * const unity_p);

#endif /* UNITY_FIXTURE_INTERNALS_H_ */
