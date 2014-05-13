/* ==========================================
    Unity Project - A Test Framework for C
    Copyright (c) 2007 Mike Karlesky, Mark VanderVoord, Greg Williams
    [Released under MIT License. Please refer to license.txt for details]
========================================== */

#include "unity.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

// VS issues a bogus warning in release builds for INFINITY. Sad.
#if defined(_MSC_VER)
#pragma warning( disable : 4756 4056 )
#endif // _MSC_VER

/* DX_PATCH: jumpless version "longjmp(unity_p->AbortFrame" removed, return added*/
#define UNITY_FAIL_AND_BAIL   { unity_p->CurrentTestFailed  = 1; UNITY_OUTPUT_CHAR('\n'); return 1; }
#define UNITY_IGNORE_AND_BAIL { unity_p->CurrentTestIgnored = 1; UNITY_OUTPUT_CHAR('\n'); return 1; }
/// return prematurely if we are already in failure or ignore state
#define UNITY_SKIP_EXECUTION  { if ((unity_p->CurrentTestFailed != 0) || (unity_p->CurrentTestIgnored != 0)) {return 1;} }
#define UNITY_PRINT_EOL       { UNITY_OUTPUT_CHAR('\n'); }

static const char* UnityStrNull     = "NULL";
static const char* UnityStrSpacer   = ". ";
static const char* UnityStrExpected = " Expected ";
static const char* UnityStrWas      = " Was ";
static const char* UnityStrTo       = " To ";
static const char* UnityStrElement  = " Element ";
static const char* UnityStrByte     = " Byte ";
static const char* UnityStrMemory   = " Memory Mismatch.";
static const char* UnityStrDelta    = " Values Not Within Delta ";
static const char* UnityStrPointless= " You Asked Me To Compare Nothing, Which Was Pointless.";
static const char* UnityStrNullPointerForExpected= " Expected pointer to be NULL";
static const char* UnityStrNullPointerForActual  = " Actual pointer was NULL";
static const char* UnityStrInf      = "Infinity";
static const char* UnityStrNegInf   = "Negative Infinity";
static const char* UnityStrNaN      = "NaN";

#ifndef UNITY_EXCLUDE_FLOAT
// Dividing by these constants produces +/- infinity.
// The rationale is given in UnityAssertFloatIsInf's body.
static const _UF f_zero = 0.0f;
#ifndef UNITY_EXCLUDE_DOUBLE
static const _UD d_zero = 0.0;
#endif
#endif

void UnityPrintFail(struct _Unity * const unity_p);
void UnityPrintOk(struct _Unity * const unity_p);

//-----------------------------------------------
// Pretty Printers & Test Result Output Handlers
//-----------------------------------------------

void UnityPrint(const char* string, struct _Unity * const unity_p)
{
    const char* pch = string;

    if (pch != NULL)
    {
        while (*pch)
        {
            // printable characters plus CR & LF are printed
            if ((*pch <= 126) && (*pch >= 32))
            {
                UNITY_OUTPUT_CHAR(*pch);
            }
            //write escaped carriage returns
            else if (*pch == 13)
            {
                UNITY_OUTPUT_CHAR('\\');
                UNITY_OUTPUT_CHAR('r');
            }
            //write escaped line feeds
            else if (*pch == 10)
            {
                UNITY_OUTPUT_CHAR('\\');
                UNITY_OUTPUT_CHAR('n');
            }
            // unprintable characters are shown as codes
            else
            {
                UNITY_OUTPUT_CHAR('\\');
                UnityPrintNumberHex((_U_SINT)*pch, 2, unity_p);
            }
            pch++;
        }
    }
}

//-----------------------------------------------
void UnityPrintNumberByStyle(const _U_SINT number, const UNITY_DISPLAY_STYLE_T style, struct _Unity * const unity_p)
{
    if ((style & UNITY_DISPLAY_RANGE_INT) == UNITY_DISPLAY_RANGE_INT)
    {
        UnityPrintNumber(number, unity_p);
    }
    else if ((style & UNITY_DISPLAY_RANGE_UINT) == UNITY_DISPLAY_RANGE_UINT)
    {
        // compiler-generic print formatting masks
        static const _U_UINT UnitySizeMask[] =
        {
            255u         // 0xFF
            ,65535u       // 0xFFFF
            ,4294967295u  // 0xFFFFFFFF
        #ifdef UNITY_SUPPORT_64
            ,0xFFFFFFFFFFFFFFFF
        #endif
        };

        const _U_UINT *maskPtr = UnitySizeMask;
         _U_UINT s = (_U_UINT)style & (_U_UINT)0x0F;
        while (s >>= 1) 
        {
            ++maskPtr;
        }
        UnityPrintNumberUnsigned(  (_U_UINT)number  &  *maskPtr , unity_p);
    }
    else
    {
        UnityPrintNumberHex((_U_UINT)number, (style & 0x000F) << 1, unity_p);
    }
}

//-----------------------------------------------
/// basically do an itoa using as little ram as possible
void UnityPrintNumber(const _U_SINT number_to_print, struct _Unity * const unity_p)
{
    _U_SINT divisor = 1;
    _U_SINT next_divisor;
    _U_SINT number = number_to_print;

    if (number < 0)
    {
        UNITY_OUTPUT_CHAR('-');
        number = -number;
    }

    // figure out initial divisor
    while (number / divisor > 9)
    {
        next_divisor = divisor * 10;
        if (next_divisor > divisor)
            divisor = next_divisor;
        else
            break;
    }

    // now mod and print, then divide divisor
    do
    {
        UNITY_OUTPUT_CHAR((char)('0' + (number / divisor % 10)));
        divisor /= 10;
    }
    while (divisor > 0);
}

//-----------------------------------------------
/// basically do an itoa using as little ram as possible
void UnityPrintNumberUnsigned(const _U_UINT number, struct _Unity * const unity_p)
{
    _U_UINT divisor = 1;
    _U_UINT next_divisor;

    // figure out initial divisor
    while (number / divisor > 9)
    {
        next_divisor = divisor * 10;
        if (next_divisor > divisor)
            divisor = next_divisor;
        else
            break;
    }

    // now mod and print, then divide divisor
    do
    {
        UNITY_OUTPUT_CHAR((char)('0' + (number / divisor % 10)));
        divisor /= 10;
    }
    while (divisor > 0);
}

//-----------------------------------------------
void UnityPrintNumberHex(const _U_UINT number, const char nibbles_to_print, struct _Unity * const unity_p)
{
    _U_UINT nibble;
    char nibbles = nibbles_to_print;
    UNITY_OUTPUT_CHAR('0');
    UNITY_OUTPUT_CHAR('x');

    while (nibbles > 0)
    {
        nibble = (number >> (--nibbles << 2)) & 0x0000000F;
        if (nibble <= 9)
        {
            UNITY_OUTPUT_CHAR((char)('0' + nibble));
        }
        else
        {
            UNITY_OUTPUT_CHAR((char)('A' - 10 + nibble));
        }
    }
}

//-----------------------------------------------
void UnityPrintMask(const _U_UINT mask, const _U_UINT number, struct _Unity * const unity_p)
{
    _U_UINT current_bit = (_U_UINT)1 << (UNITY_INT_WIDTH - 1);
    _US32 i;

    for (i = 0; i < UNITY_INT_WIDTH; i++)
    {
        if (current_bit & mask)
        {
            if (current_bit & number)
            {
                UNITY_OUTPUT_CHAR('1');
            }
            else
            {
                UNITY_OUTPUT_CHAR('0');
            }
        }
        else
        {
            UNITY_OUTPUT_CHAR('X');
        }
        current_bit = current_bit >> 1;
    }
}

//-----------------------------------------------
#ifdef UNITY_FLOAT_VERBOSE
void UnityPrintFloat(const _UF number, struct _Unity * const unity_p)
{
    char TempBuffer[32];
    sprintf(TempBuffer, "%.6f", number);
    UnityPrint(TempBuffer, unity_p);
}
#endif

//-----------------------------------------------

void UnityPrintFail( struct _Unity * const unity_p )
{
    UnityPrint("FAIL", unity_p);
}

void UnityPrintOk( struct _Unity * const unity_p )
{
    UnityPrint("OK", unity_p);
}

//-----------------------------------------------
/* DX_PATCH: "static" modifier required to avoid GCC warning: no previous prototype for... */
static void UnityTestResultsBegin(const char* file, const UNITY_LINE_TYPE line, struct _Unity * const unity_p)
{
    UNITY_PRINT_EOL;
    UnityPrint(file, unity_p);
    UNITY_OUTPUT_CHAR(':');
    UnityPrintNumber(line, unity_p);
    UNITY_OUTPUT_CHAR(':');
    UnityPrint(unity_p->CurrentTestName, unity_p);
    UNITY_OUTPUT_CHAR(':');
}

//-----------------------------------------------
/* DX_PATCH: "static" modifier required to avoid GCC warning: no previous prototype for... */
static void UnityTestResultsFailBegin(const UNITY_LINE_TYPE line, struct _Unity * const unity_p)
{
    UnityTestResultsBegin(unity_p->TestFile, line, unity_p);
    UnityPrint("FAIL:", unity_p);
}

//-----------------------------------------------
void UnityConcludeTest( struct _Unity * const unity_p )
{
    if (unity_p->CurrentTestIgnored)
    {
        unity_p->TestIgnores++;
    }
    else if (!unity_p->CurrentTestFailed)
    {
        UnityTestResultsBegin(unity_p->TestFile, unity_p->CurrentTestLineNumber, unity_p);
        UnityPrint("PASS", unity_p);
        UNITY_PRINT_EOL;
    }
    else
    {
        unity_p->TestFailures++;
    }

    unity_p->CurrentTestFailed = 0;
    unity_p->CurrentTestIgnored = 0;
}

//-----------------------------------------------
/* DX_PATCH: "static" modifier required to avoid GCC warning: no previous prototype for... */
static void UnityAddMsgIfSpecified(const char* msg, struct _Unity * const unity_p)
{
    if (msg)
    {
        UnityPrint(UnityStrSpacer, unity_p);
        UnityPrint(msg, unity_p);
    }
}

//-----------------------------------------------
/* DX_PATCH: "static" modifier required to avoid GCC warning: no previous prototype for... */
static void UnityPrintExpectedAndActualStrings(const char* expected, const char* actual, struct _Unity * const unity_p)
{
    UnityPrint(UnityStrExpected, unity_p);
    if (expected != NULL)
    {
        UNITY_OUTPUT_CHAR('\'');
        UnityPrint(expected, unity_p);
        UNITY_OUTPUT_CHAR('\'');
    }
    else
    {
      UnityPrint(UnityStrNull, unity_p);
    }
    UnityPrint(UnityStrWas, unity_p);
    if (actual != NULL)
    {
        UNITY_OUTPUT_CHAR('\'');
        UnityPrint(actual, unity_p);
        UNITY_OUTPUT_CHAR('\'');
    }
    else
    {
      UnityPrint(UnityStrNull, unity_p);
    }
}

//-----------------------------------------------
// Assertion & Control Helpers
//-----------------------------------------------

/* DX_PATCH: "static" modifier required to avoid GCC warning: no previous prototype for... */
static int UnityCheckArraysForNull(UNITY_PTR_ATTRIBUTE const void* expected, UNITY_PTR_ATTRIBUTE const void* actual, const UNITY_LINE_TYPE lineNumber, const char* msg, struct _Unity * const unity_p)
{
    //return true if they are both NULL
    if ((expected == NULL) && (actual == NULL))
        return 1;

    //throw error if just expected is NULL
    if (expected == NULL)
    {
        UnityTestResultsFailBegin(lineNumber, unity_p);
        UnityPrint(UnityStrNullPointerForExpected, unity_p);
        UnityAddMsgIfSpecified(msg, unity_p);
        UNITY_FAIL_AND_BAIL;
    }

    //throw error if just actual is NULL
    if (actual == NULL)
    {
        UnityTestResultsFailBegin(lineNumber, unity_p);
        UnityPrint(UnityStrNullPointerForActual, unity_p);
        UnityAddMsgIfSpecified(msg, unity_p);
        UNITY_FAIL_AND_BAIL;
    }

    //return false if neither is NULL
    return 0;
}

//-----------------------------------------------
// Assertion Functions
//-----------------------------------------------

/* DX_PATCH: jumpless version. All assertion functions return UNITY_BOOL value instead of void*/
bool UnityAssertBits(const _U_SINT mask,
                     const _U_SINT expected,
                     const _U_SINT actual,
                     const char* msg,
                     const UNITY_LINE_TYPE lineNumber, const char *file, struct _Unity * const unity_p)
{
    UNITY_SKIP_EXECUTION;

    if ((mask & expected) != (mask & actual))
    {
        unity_p->TestFile = file;
        UnityTestResultsFailBegin(lineNumber, unity_p);
        UnityPrint(UnityStrExpected, unity_p);
        UnityPrintMask(mask, expected, unity_p);
        UnityPrint(UnityStrWas, unity_p);
        UnityPrintMask(mask, actual, unity_p);
        UnityAddMsgIfSpecified(msg, unity_p);
        UNITY_FAIL_AND_BAIL;
    }
    return false;
}

//-----------------------------------------------
bool UnityAssertEqualNumber(const _U_SINT expected,
                            const _U_SINT actual,
                            const char* msg,
                            const UNITY_LINE_TYPE lineNumber, const char *file,
                            const UNITY_DISPLAY_STYLE_T style, struct _Unity * const unity_p)
{
    UNITY_SKIP_EXECUTION;

    if (expected != actual)
    {
        unity_p->TestFile = file;
        UnityTestResultsFailBegin(lineNumber, unity_p);
        UnityPrint(UnityStrExpected, unity_p);
        UnityPrintNumberByStyle(expected, style, unity_p);
        UnityPrint(UnityStrWas, unity_p);
        UnityPrintNumberByStyle(actual, style, unity_p);
        UnityAddMsgIfSpecified(msg, unity_p);
        UNITY_FAIL_AND_BAIL;
    }
    return false;
}

//-----------------------------------------------
bool UnityAssertEqualIntArray(UNITY_PTR_ATTRIBUTE const void* expected,
                              UNITY_PTR_ATTRIBUTE const void* actual,
                              const _UU32 num_elements,
                              const char* msg,
                              const UNITY_LINE_TYPE lineNumber, const char *file,
                              const UNITY_DISPLAY_STYLE_T style, struct _Unity * const unity_p)
{
    _UU32 elements = num_elements;
    UNITY_PTR_ATTRIBUTE const _US8* ptr_exp = (UNITY_PTR_ATTRIBUTE _US8*)expected;
    UNITY_PTR_ATTRIBUTE const _US8* ptr_act = (UNITY_PTR_ATTRIBUTE _US8*)actual;

    UNITY_SKIP_EXECUTION;

    unity_p->TestFile = file;
    if (elements == 0)
    {
        UnityTestResultsFailBegin(lineNumber, unity_p);
        UnityPrint(UnityStrPointless, unity_p);
        UnityAddMsgIfSpecified(msg, unity_p);
        UNITY_FAIL_AND_BAIL;
    }

    if (UnityCheckArraysForNull((UNITY_PTR_ATTRIBUTE void*)expected, (UNITY_PTR_ATTRIBUTE void*)actual, lineNumber, msg, unity_p) == 1)
        return true;

    // If style is UNITY_DISPLAY_STYLE_INT, we'll fall into the default case rather than the INT16 or INT32 (etc) case
    // as UNITY_DISPLAY_STYLE_INT includes a flag for UNITY_DISPLAY_RANGE_AUTO, which the width-specific
    // variants do not. Therefore remove this flag.
    switch(style & ~UNITY_DISPLAY_RANGE_AUTO)
    {
        case UNITY_DISPLAY_STYLE_HEX8:
        case UNITY_DISPLAY_STYLE_INT8:
        case UNITY_DISPLAY_STYLE_UINT8:
            while (elements--)
            {
                if (*ptr_exp != *ptr_act)
                {
                    UnityTestResultsFailBegin(lineNumber, unity_p);
                    UnityPrint(UnityStrElement, unity_p);
                    UnityPrintNumberByStyle((num_elements - elements - 1), UNITY_DISPLAY_STYLE_UINT, unity_p);
                    UnityPrint(UnityStrExpected, unity_p);
                    UnityPrintNumberByStyle(*ptr_exp, style, unity_p);
                    UnityPrint(UnityStrWas, unity_p);
                    UnityPrintNumberByStyle(*ptr_act, style, unity_p);
                    UnityAddMsgIfSpecified(msg, unity_p);
                    UNITY_FAIL_AND_BAIL;
                }
                ptr_exp += 1;
                ptr_act += 1;
            }
            break;
        case UNITY_DISPLAY_STYLE_HEX16:
        case UNITY_DISPLAY_STYLE_INT16:
        case UNITY_DISPLAY_STYLE_UINT16:
            while (elements--)
            {
                if (*(UNITY_PTR_ATTRIBUTE _US16*)ptr_exp != *(UNITY_PTR_ATTRIBUTE _US16*)ptr_act)
                {
                    UnityTestResultsFailBegin(lineNumber, unity_p);
                    UnityPrint(UnityStrElement, unity_p);
                    UnityPrintNumberByStyle((num_elements - elements - 1), UNITY_DISPLAY_STYLE_UINT, unity_p);
                    UnityPrint(UnityStrExpected, unity_p);
                    UnityPrintNumberByStyle(*(UNITY_PTR_ATTRIBUTE _US16*)ptr_exp, style, unity_p);
                    UnityPrint(UnityStrWas, unity_p);
                    UnityPrintNumberByStyle(*(UNITY_PTR_ATTRIBUTE _US16*)ptr_act, style, unity_p);
                    UnityAddMsgIfSpecified(msg, unity_p);
                    UNITY_FAIL_AND_BAIL;
                }
                ptr_exp += 2;
                ptr_act += 2;
            }
            break;
#ifdef UNITY_SUPPORT_64
        case UNITY_DISPLAY_STYLE_HEX64:
        case UNITY_DISPLAY_STYLE_INT64:
        case UNITY_DISPLAY_STYLE_UINT64:
            while (elements--)
            {
                if (*(UNITY_PTR_ATTRIBUTE _US64*)ptr_exp != *(UNITY_PTR_ATTRIBUTE _US64*)ptr_act)
                {
                    UnityTestResultsFailBegin(lineNumber, unity_p);
                    UnityPrint(UnityStrElement, unity_p);
                    UnityPrintNumberByStyle((num_elements - elements - 1), UNITY_DISPLAY_STYLE_UINT, unity_p);
                    UnityPrint(UnityStrExpected, unity_p);
                    UnityPrintNumberByStyle(*(UNITY_PTR_ATTRIBUTE _US64*)ptr_exp, style, unity_p);
                    UnityPrint(UnityStrWas, unity_p);
                    UnityPrintNumberByStyle(*(UNITY_PTR_ATTRIBUTE _US64*)ptr_act, style, unity_p);
                    UnityAddMsgIfSpecified(msg, unity_p);
                    UNITY_FAIL_AND_BAIL;
                }
                ptr_exp += 8;
                ptr_act += 8;
            }
            break;
#endif
        default:
            while (elements--)
            {
                if (*(UNITY_PTR_ATTRIBUTE _US32*)ptr_exp != *(UNITY_PTR_ATTRIBUTE _US32*)ptr_act)
                {
                    UnityTestResultsFailBegin(lineNumber, unity_p);
                    UnityPrint(UnityStrElement, unity_p);
                    UnityPrintNumberByStyle((num_elements - elements - 1), UNITY_DISPLAY_STYLE_UINT, unity_p);
                    UnityPrint(UnityStrExpected, unity_p);
                    UnityPrintNumberByStyle(*(UNITY_PTR_ATTRIBUTE _US32*)ptr_exp, style, unity_p);
                    UnityPrint(UnityStrWas, unity_p);
                    UnityPrintNumberByStyle(*(UNITY_PTR_ATTRIBUTE _US32*)ptr_act, style, unity_p);
                    UnityAddMsgIfSpecified(msg, unity_p);
                    UNITY_FAIL_AND_BAIL;
                }
                ptr_exp += 4;
                ptr_act += 4;
            }
            break;
    }
    return false;
}

//-----------------------------------------------
#ifndef UNITY_EXCLUDE_FLOAT
bool UnityAssertEqualFloatArray(UNITY_PTR_ATTRIBUTE const _UF* expected,
                                UNITY_PTR_ATTRIBUTE const _UF* actual,
                                const _UU32 num_elements,
                                const char* msg,
                                const UNITY_LINE_TYPE lineNumber, const char *file, struct _Unity * const unity_p)
{
    _UU32 elements = num_elements;
    UNITY_PTR_ATTRIBUTE const _UF* ptr_expected = expected;
    UNITY_PTR_ATTRIBUTE const _UF* ptr_actual = actual;
    _UF diff, tol;

    UNITY_SKIP_EXECUTION;

    unity_p->TestFile = file;

    if (elements == 0)
    {
        UnityTestResultsFailBegin(lineNumber, unity_p);
        UnityPrint(UnityStrPointless, unity_p);
        UnityAddMsgIfSpecified(msg, unity_p);
        UNITY_FAIL_AND_BAIL;
    }

    if (UnityCheckArraysForNull((UNITY_PTR_ATTRIBUTE void*)expected, (UNITY_PTR_ATTRIBUTE void*)actual, lineNumber, msg, unity_p) == 1)
        return true;

    while (elements--)
    {
        diff = *ptr_expected - *ptr_actual;
        if (diff < 0.0f)
          diff = 0.0f - diff;
        tol = UNITY_FLOAT_PRECISION * *ptr_expected;
        if (tol < 0.0f)
            tol = 0.0f - tol;

        //This first part of this condition will catch any NaN or Infinite values
        if ((diff * 0.0f != 0.0f) || (diff > tol))
        {
            UnityTestResultsFailBegin(lineNumber, unity_p);
            UnityPrint(UnityStrElement, unity_p);
            UnityPrintNumberByStyle((num_elements - elements - 1), UNITY_DISPLAY_STYLE_UINT, unity_p);
#ifdef UNITY_FLOAT_VERBOSE
            UnityPrint(UnityStrExpected, unity_p);
            UnityPrintFloat(*ptr_expected, unity_p);
            UnityPrint(UnityStrWas, unity_p);
            UnityPrintFloat(*ptr_actual, unity_p);
#else
            UnityPrint(UnityStrDelta, unity_p);
#endif
            UnityAddMsgIfSpecified(msg, unity_p);
            UNITY_FAIL_AND_BAIL;
        }
        ptr_expected++;
        ptr_actual++;
    }
    return false;
}

//-----------------------------------------------
bool UnityAssertFloatsWithin(const _UF delta,
                             const _UF expected,
                             const _UF actual,
                             const char* msg,
                             const UNITY_LINE_TYPE lineNumber, const char *file, struct _Unity * const unity_p)
{
    _UF diff = actual - expected;
    _UF pos_delta = delta;

    UNITY_SKIP_EXECUTION;

    if (diff < 0.0f)
    {
        diff = 0.0f - diff;
    }
    if (pos_delta < 0.0f)
    {
        pos_delta = 0.0f - pos_delta;
    }

    //This first part of this condition will catch any NaN or Infinite values
    if ((diff * 0.0f != 0.0f) || (pos_delta < diff))
    {
        unity_p->TestFile = file;
        UnityTestResultsFailBegin(lineNumber, unity_p);
#ifdef UNITY_FLOAT_VERBOSE
        UnityPrint(UnityStrExpected, unity_p);
        UnityPrintFloat(expected, unity_p);
        UnityPrint(UnityStrWas, unity_p);
        UnityPrintFloat(actual, unity_p);
#else
        UnityPrint(UnityStrDelta, unity_p);
#endif
        UnityAddMsgIfSpecified(msg, unity_p);
        UNITY_FAIL_AND_BAIL;
    }
    return false;
}

//-----------------------------------------------
bool UnityAssertFloatIsInf(const _UF actual,
                           const char* msg,
                           const UNITY_LINE_TYPE lineNumber, const char *file, struct _Unity * const unity_p)
{
    UNITY_SKIP_EXECUTION;

    // In Microsoft Visual C++ Express Edition 2008,
    //   if ((1.0f / f_zero) != actual)
    // produces
    //   error C2124: divide or mod by zero
    // As a workaround, place 0 into a variable.
    if (INFINITY != actual)
    {
        unity_p->TestFile = file;

        UnityTestResultsFailBegin(lineNumber, unity_p);
#ifdef UNITY_FLOAT_VERBOSE
        UnityPrint(UnityStrExpected, unity_p);
        UnityPrint(UnityStrInf, unity_p);
        UnityPrint(UnityStrWas, unity_p);
        UnityPrintFloat(actual, unity_p);
#else
        UnityPrint(UnityStrDelta, unity_p);
#endif
        UnityAddMsgIfSpecified(msg, unity_p);
        UNITY_FAIL_AND_BAIL;
    }
    return false;
}

//-----------------------------------------------
bool UnityAssertFloatIsNegInf(const _UF actual,
                              const char* msg,
                              const UNITY_LINE_TYPE lineNumber, const char *file, struct _Unity * const unity_p)
{
    UNITY_SKIP_EXECUTION;

    // The rationale for not using 1.0f/0.0f is given in UnityAssertFloatIsInf's body.
    if (-INFINITY != actual)
    {
        unity_p->TestFile = file;

        UnityTestResultsFailBegin(lineNumber, unity_p);
#ifdef UNITY_FLOAT_VERBOSE
        UnityPrint(UnityStrExpected, unity_p);
        UnityPrint(UnityStrNegInf, unity_p);
        UnityPrint(UnityStrWas, unity_p);
        UnityPrintFloat(actual, unity_p);
#else
        UnityPrint(UnityStrDelta, unity_p);
#endif
        UnityAddMsgIfSpecified(msg, unity_p);
        UNITY_FAIL_AND_BAIL;
    }
    return false;
}

//-----------------------------------------------
bool UnityAssertFloatIsNaN(const _UF actual,
                           const char* msg,
                           const UNITY_LINE_TYPE lineNumber, const char *file, struct _Unity * const unity_p)
{
    UNITY_SKIP_EXECUTION;

    if (actual == actual)
    {
        unity_p->TestFile = file;

        UnityTestResultsFailBegin(lineNumber, unity_p);
#ifdef UNITY_FLOAT_VERBOSE
        UnityPrint(UnityStrExpected, unity_p);
        UnityPrint(UnityStrNaN, unity_p);
        UnityPrint(UnityStrWas, unity_p);
        UnityPrintFloat(actual, unity_p);
#else
        UnityPrint(UnityStrDelta, unity_p);
#endif
        UnityAddMsgIfSpecified(msg, unity_p);
        UNITY_FAIL_AND_BAIL;
    }
    return false;
}

#endif //not UNITY_EXCLUDE_FLOAT

//-----------------------------------------------
#ifndef UNITY_EXCLUDE_DOUBLE
bool UnityAssertEqualDoubleArray(UNITY_PTR_ATTRIBUTE const _UD* expected,
                                 UNITY_PTR_ATTRIBUTE const _UD* actual,
                                 const _UU32 num_elements,
                                 const char* msg,
                                 const UNITY_LINE_TYPE lineNumber, const char *file, struct _Unity * const unity_p)
{
    _UU32 elements = num_elements;
    UNITY_PTR_ATTRIBUTE const _UD* ptr_expected = expected;
    UNITY_PTR_ATTRIBUTE const _UD* ptr_actual = actual;
    _UD diff, tol;

    UNITY_SKIP_EXECUTION;

    unity_p->TestFile = file;

    if (elements == 0)
    {
        UnityTestResultsFailBegin(lineNumber, unity_p);
        UnityPrint(UnityStrPointless, unity_p);
        UnityAddMsgIfSpecified(msg, unity_p);
        UNITY_FAIL_AND_BAIL;
    }

    if (UnityCheckArraysForNull((UNITY_PTR_ATTRIBUTE void*)expected, (UNITY_PTR_ATTRIBUTE void*)actual, lineNumber, msg, unity_p) == 1)
        return true;

    while (elements--)
    {
        diff = *ptr_expected - *ptr_actual;
        if (diff < 0.0)
          diff = 0.0 - diff;
        tol = UNITY_DOUBLE_PRECISION * *ptr_expected;
        if (tol < 0.0)
            tol = 0.0 - tol;

        //This first part of this condition will catch any NaN or Infinite values
        if ((diff * 0.0 != 0.0) || (diff > tol))
        {
            UnityTestResultsFailBegin(lineNumber, unity_p);
            UnityPrint(UnityStrElement, unity_p);
            UnityPrintNumberByStyle((num_elements - elements - 1), UNITY_DISPLAY_STYLE_UINT, unity_p);
#ifdef UNITY_DOUBLE_VERBOSE
            UnityPrint(UnityStrExpected, unity_p);
            UnityPrintFloat((float)(*ptr_expected), unity_p);
            UnityPrint(UnityStrWas, unity_p);
            UnityPrintFloat((float)(*ptr_actual), unity_p);
#else
            UnityPrint(UnityStrDelta, unity_p);
#endif
            UnityAddMsgIfSpecified(msg, unity_p);
            UNITY_FAIL_AND_BAIL;
        }
        ptr_expected++;
        ptr_actual++;
    }
    return false;
}

//-----------------------------------------------
bool UnityAssertDoublesWithin(const _UD delta,
                              const _UD expected,
                              const _UD actual,
                              const char* msg,
                              const UNITY_LINE_TYPE lineNumber, const char *file, struct _Unity * const unity_p)
{
    _UD diff = actual - expected;
    _UD pos_delta = delta;

    UNITY_SKIP_EXECUTION;

    if (diff < 0.0)
    {
        diff = 0.0 - diff;
    }
    if (pos_delta < 0.0)
    {
        pos_delta = 0.0 - pos_delta;
    }

    //This first part of this condition will catch any NaN or Infinite values
    if ((diff * 0.0 != 0.0) || (pos_delta < diff))
    {
        unity_p->TestFile = file;

        UnityTestResultsFailBegin(lineNumber, unity_p);
#ifdef UNITY_DOUBLE_VERBOSE
        UnityPrint(UnityStrExpected, unity_p);
        UnityPrintFloat((float)expected, unity_p);
        UnityPrint(UnityStrWas, unity_p);
        UnityPrintFloat((float)actual, unity_p);
#else
        UnityPrint(UnityStrDelta, unity_p);
#endif
        UnityAddMsgIfSpecified(msg, unity_p);
        UNITY_FAIL_AND_BAIL;
    }
    return false;
}

//-----------------------------------------------
bool UnityAssertDoubleIsInf(const _UD actual,
                            const char* msg,
                            const UNITY_LINE_TYPE lineNumber, struct _Unity * const unity_p)
{
    UNITY_SKIP_EXECUTION;

    // The rationale for not using 1.0/0.0 is given in UnityAssertFloatIsInf's body.
    if ((1.0 / d_zero) != actual)
    {
        UnityTestResultsFailBegin(lineNumber, unity_p);
#ifdef UNITY_DOUBLE_VERBOSE
        UnityPrint(UnityStrExpected, unity_p);
        UnityPrint(UnityStrInf, unity_p);
        UnityPrint(UnityStrWas, unity_p);
        UnityPrintFloat((float)actual, unity_p);
#else
        UnityPrint(UnityStrDelta, unity_p);
#endif
        UnityAddMsgIfSpecified(msg, unity_p);
        UNITY_FAIL_AND_BAIL;
    }
    return false;
}

//-----------------------------------------------
bool UnityAssertDoubleIsNegInf(const _UD actual,
                               const char* msg,
                               const UNITY_LINE_TYPE lineNumber, struct _Unity * const unity_p)
{
    UNITY_SKIP_EXECUTION;

    // The rationale for not using 1.0/0.0 is given in UnityAssertFloatIsInf's body.
    if ((-1.0 / d_zero) != actual)
    {
        UnityTestResultsFailBegin(lineNumber, unity_p);
#ifdef UNITY_DOUBLE_VERBOSE
        UnityPrint(UnityStrExpected, unity_p);
        UnityPrint(UnityStrNegInf, unity_p);
        UnityPrint(UnityStrWas, unity_p);
        UnityPrintFloat((float)actual, unity_p);
#else
        UnityPrint(UnityStrDelta, unity_p);
#endif
        UnityAddMsgIfSpecified(msg, unity_p);
        UNITY_FAIL_AND_BAIL;
    }
    return false;
}

//-----------------------------------------------
bool UnityAssertDoubleIsNaN(const _UD actual,
                            const char* msg,
                            const UNITY_LINE_TYPE lineNumber, struct _Unity * const unity_p)
{
    UNITY_SKIP_EXECUTION;

    if (actual == actual)
    {
        UnityTestResultsFailBegin(lineNumber, unity_p);
#ifdef UNITY_DOUBLE_VERBOSE
        UnityPrint(UnityStrExpected, unity_p);
        UnityPrint(UnityStrNaN, unity_p);
        UnityPrint(UnityStrWas, unity_p);
        UnityPrintFloat((float)actual, unity_p);
#else
        UnityPrint(UnityStrDelta, unity_p);
#endif
        UnityAddMsgIfSpecified(msg, unity_p);
        UNITY_FAIL_AND_BAIL;
    }
    return false;
}

#endif // not UNITY_EXCLUDE_DOUBLE

//-----------------------------------------------
bool UnityAssertNumbersWithin( const _U_SINT delta,
                               const _U_SINT expected,
                               const _U_SINT actual,
                               const char* msg,
                               const UNITY_LINE_TYPE lineNumber, const char *file,
                               const UNITY_DISPLAY_STYLE_T style, struct _Unity * const unity_p)
{
    UNITY_SKIP_EXECUTION;

    unity_p->TestFile = file;

    if ((style & UNITY_DISPLAY_RANGE_INT) == UNITY_DISPLAY_RANGE_INT)
    {
        if (actual > expected)
          unity_p->CurrentTestFailed = ((actual - expected) > delta);
        else
          unity_p->CurrentTestFailed = ((expected - actual) > delta);
    }
    else
    {
        if ((_U_UINT)actual > (_U_UINT)expected)
            unity_p->CurrentTestFailed = ((_U_UINT)(actual - expected) > (_U_UINT)delta);
        else
            unity_p->CurrentTestFailed = ((_U_UINT)(expected - actual) > (_U_UINT)delta);
    }

    if (unity_p->CurrentTestFailed)
    {
        UnityTestResultsFailBegin(lineNumber, unity_p);
        UnityPrint(UnityStrDelta, unity_p);
        UnityPrintNumberByStyle(delta, style, unity_p);
        UnityPrint(UnityStrExpected, unity_p);
        UnityPrintNumberByStyle(expected, style, unity_p);
        UnityPrint(UnityStrWas, unity_p);
        UnityPrintNumberByStyle(actual, style, unity_p);
        UnityAddMsgIfSpecified(msg, unity_p);
        UNITY_FAIL_AND_BAIL;
    }
    return false;
}

//-----------------------------------------------
bool UnityAssertEqualString(const char* expected,
                            const char* actual,
                            const char* msg,
                            const UNITY_LINE_TYPE lineNumber, const char *file, struct _Unity * const unity_p)
{
    _UU32 i;

    UNITY_SKIP_EXECUTION;

    unity_p->TestFile = file;

    // if both pointers not null compare the strings
    if (expected && actual)
    {
        for (i = 0; expected[i] || actual[i]; i++)
        {
            if (expected[i] != actual[i])
            {
                unity_p->CurrentTestFailed = 1;
                break;
            }
        }
    }
    else
    { // handle case of one pointers being null (if both null, test should pass)
        if (expected != actual)
        {
            unity_p->CurrentTestFailed = 1;
        }
    }

    if (unity_p->CurrentTestFailed)
    {
      UnityTestResultsFailBegin(lineNumber, unity_p);
      UnityPrintExpectedAndActualStrings(expected, actual, unity_p);
      UnityAddMsgIfSpecified(msg, unity_p);
      UNITY_FAIL_AND_BAIL;
    }
    return false;
}

//-----------------------------------------------
bool UnityAssertEqualStringArray( const char** expected,
                                  const char** actual,
                                  const _UU32 num_elements,
                                  const char* msg,
                                  const UNITY_LINE_TYPE lineNumber, struct _Unity * const unity_p)
{
    _UU32 i, j = 0;

    UNITY_SKIP_EXECUTION;

    // if no elements, it's an error
    if (num_elements == 0)
    {
        UnityTestResultsFailBegin(lineNumber, unity_p);
        UnityPrint(UnityStrPointless, unity_p);
        UnityAddMsgIfSpecified(msg, unity_p);
        UNITY_FAIL_AND_BAIL;
    }

    if (UnityCheckArraysForNull((UNITY_PTR_ATTRIBUTE void*)expected, (UNITY_PTR_ATTRIBUTE void*)actual, lineNumber, msg, unity_p) == 1)
        return true;

    do
    {
        // if both pointers not null compare the strings
        if (expected[j] && actual[j])
        {
            for (i = 0; expected[j][i] || actual[j][i]; i++)
            {
                if (expected[j][i] != actual[j][i])
                {
                    unity_p->CurrentTestFailed = 1;
                    break;
                }
            }
        }
        else
        { // handle case of one pointers being null (if both null, test should pass)
            if (expected[j] != actual[j])
            {
                unity_p->CurrentTestFailed = 1;
            }
        }

        if (unity_p->CurrentTestFailed)
        {
            UnityTestResultsFailBegin(lineNumber, unity_p);
            if (num_elements > 1)
            {
                UnityPrint(UnityStrElement, unity_p);
                UnityPrintNumberByStyle((j), UNITY_DISPLAY_STYLE_UINT, unity_p);
            }
            UnityPrintExpectedAndActualStrings((const char*)(expected[j]), (const char*)(actual[j]), unity_p);
            UnityAddMsgIfSpecified(msg, unity_p);
            UNITY_FAIL_AND_BAIL;
        }
    } while (++j < num_elements);

    return false;
}

//-----------------------------------------------
bool UnityAssertEqualMemory( UNITY_PTR_ATTRIBUTE const void* expected,
                             UNITY_PTR_ATTRIBUTE const void* actual,
                             const _UU32 length,
                             const _UU32 num_elements,
                             const char* msg,
                             const UNITY_LINE_TYPE lineNumber, const char *file, struct _Unity * const unity_p)
{
    UNITY_PTR_ATTRIBUTE unsigned char* ptr_exp = (UNITY_PTR_ATTRIBUTE unsigned char*)expected;
    UNITY_PTR_ATTRIBUTE unsigned char* ptr_act = (UNITY_PTR_ATTRIBUTE unsigned char*)actual;
    _UU32 elements = num_elements;
    _UU32 bytes;

    UNITY_SKIP_EXECUTION;

    unity_p->TestFile = file;

    if ((elements == 0) || (length == 0))
    {
        UnityTestResultsFailBegin(lineNumber, unity_p);
        UnityPrint(UnityStrPointless, unity_p);
        UnityAddMsgIfSpecified(msg, unity_p);
        UNITY_FAIL_AND_BAIL;
    }

    if (UnityCheckArraysForNull((UNITY_PTR_ATTRIBUTE void*)expected, (UNITY_PTR_ATTRIBUTE void*)actual, lineNumber, msg, unity_p) == 1)
        return true;

    while (elements--)
    {
        /////////////////////////////////////
        bytes = length;
        while (bytes--)
        {
            if (*ptr_exp != *ptr_act)
            {
                UnityTestResultsFailBegin(lineNumber, unity_p);
                UnityPrint(UnityStrMemory, unity_p);
                if (num_elements > 1)
                {
                    UnityPrint(UnityStrElement, unity_p);
                    UnityPrintNumberByStyle((num_elements - elements - 1), UNITY_DISPLAY_STYLE_UINT, unity_p);
                }
                UnityPrint(UnityStrByte, unity_p);
                UnityPrintNumberByStyle((length - bytes - 1), UNITY_DISPLAY_STYLE_UINT, unity_p);
                UnityPrint(UnityStrExpected, unity_p);
                UnityPrintNumberByStyle(*ptr_exp, UNITY_DISPLAY_STYLE_HEX8, unity_p);
                UnityPrint(UnityStrWas, unity_p);
                UnityPrintNumberByStyle(*ptr_act, UNITY_DISPLAY_STYLE_HEX8, unity_p);
                UnityAddMsgIfSpecified(msg, unity_p);
                UNITY_FAIL_AND_BAIL;
            }
            ptr_exp += 1;
            ptr_act += 1;
        }
        /////////////////////////////////////

    }
    return false;
}

//-----------------------------------------------
// Control Functions
//-----------------------------------------------

/* DX_PATCH: skipExecutionOnFailure parameter added, to control whether an assert message is printed
   even if one was printed before. */
bool UnityFail(const char* message, const UNITY_LINE_TYPE line, const char *file, bool skipExecutionOnFailure, struct _Unity * const unity_p)
{
    if (skipExecutionOnFailure) {
        UNITY_SKIP_EXECUTION;
    }

    unity_p->TestFile = file;
    UnityTestResultsBegin(unity_p->TestFile, line, unity_p);
    UnityPrintFail(unity_p);
    if (message != NULL)
    {
      UNITY_OUTPUT_CHAR(':');
      if (message[0] != ' ')
      {
        UNITY_OUTPUT_CHAR(' ');
      }
      UnityPrint(message, unity_p);
    }
    UNITY_FAIL_AND_BAIL;
}

//-----------------------------------------------
bool UnityIgnore(const char* message, const UNITY_LINE_TYPE line, const char *file, struct _Unity * const unity_p)
{
    UNITY_SKIP_EXECUTION;

    UnityTestResultsBegin(unity_p->TestFile, line, unity_p);
    UnityPrint("IGNORE", unity_p);
    if (message != NULL)
    {
      UNITY_OUTPUT_CHAR(':');
      UNITY_OUTPUT_CHAR(' ');
      UnityPrint(message, unity_p);
    }
    UNITY_IGNORE_AND_BAIL;
}

//-----------------------------------------------
void setUp(struct _Unity * const unity_p);
void tearDown(struct _Unity * const unity_p);

void UnityDefaultTestRun(UnityTestFunction Func, const char* FuncName, const int FuncLineNum, struct _Unity * const unity_p)
{
    unity_p->CurrentTestName = FuncName;
    unity_p->CurrentTestLineNumber = FuncLineNum;
    unity_p->NumberOfTests++;
    if (TEST_PROTECT())
    {
        setUp(unity_p);
        Func(unity_p);
    }
    if (TEST_PROTECT() && !(unity_p->CurrentTestIgnored))
    {
        tearDown(unity_p);
    }
    UnityConcludeTest(unity_p);
}

//-----------------------------------------------
void UnityBegin( struct _Unity * const unity_p )
{
    unity_p->TestFile = NULL;
    unity_p->CurrentTestName = NULL;
    unity_p->CurrentTestLineNumber = 0;
    unity_p->NumberOfTests = 0;
    unity_p->TestFailures = 0;
    unity_p->TestIgnores = 0;
    unity_p->CurrentTestFailed = 0;
    unity_p->CurrentTestIgnored = 0;
    unity_p->testLocalStorage = NULL;
}

//-----------------------------------------------
int UnityEnd( struct _Unity * const unity_p )
{
    UnityPrint("-----------------------", unity_p);
    UNITY_PRINT_EOL;
    UnityPrintNumber(unity_p->NumberOfTests, unity_p);
    UnityPrint(" Tests ", unity_p);
    UnityPrintNumber(unity_p->TestFailures, unity_p);
    UnityPrint(" Failures ", unity_p);
    UnityPrintNumber(unity_p->TestIgnores, unity_p);
    UnityPrint(" Ignored", unity_p);
    UNITY_PRINT_EOL;
    if (unity_p->TestFailures == 0U)
    {
        UnityPrintOk(unity_p);
    }
    else
    {
        UnityPrintFail(unity_p);
    }
    UNITY_PRINT_EOL;
    return unity_p->TestFailures;
}
