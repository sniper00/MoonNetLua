#ifndef _ATOMIC_H___
#define _ATOMIC_H___

#if defined(_MSC_VER)
#include <stdbool.h>
#include <stdint.h>
#include <intrin.h>

#define atomic_init(object, value)  (void)(*(object) = (value))
#define atomic_load(object)             *(object)
#define atomic_store(object, desired)   (void)(*(object) = (desired))

typedef char                atomic_schar;
typedef unsigned char       atomic_uchar;
typedef intptr_t            atomic_intptr_t;
typedef uintptr_t           atomic_uintptr_t;
typedef size_t              atomic_size_t;
typedef short               atomic_short;
typedef int                 atomic_int;
typedef long                atomic_long;
typedef long long           atomic_llong;

inline bool atomic_compare_exchange8_(volatile atomic_schar* object, int8_t* expected, int8_t desired)
{
    int8_t comparand = *expected;
    const int8_t dstValue = _InterlockedCompareExchange8(object, desired, comparand);
    bool success = dstValue == comparand;
    if (!success)
        *expected = dstValue;

    return success;
}

inline bool atomic_compare_exchange16_(volatile atomic_short* object, int16_t* expected, int16_t desired)
{
    int16_t comparand = *expected;
    const int16_t dstValue = _InterlockedCompareExchange16(object, desired, comparand);
    bool success = dstValue == comparand;
    if (!success)
        *expected = dstValue;

    return success;
}

inline bool atomic_compare_exchange32_(volatile atomic_long* object, int32_t* expected, int32_t desired)
{
    int32_t comparand = *expected;
    int32_t dstValue = _InterlockedCompareExchange(object, desired, comparand);
    bool success = dstValue == comparand;
    if (!success)
        *expected = dstValue;

    return success;
}

inline bool atomic_compare_exchange64_(volatile atomic_llong* object, int64_t* expected, int64_t desired)
{
    int64_t comparand = *expected;
    int64_t dstValue = _InterlockedCompareExchange64(object, desired, comparand);
    bool success = dstValue == comparand;
    if (!success)
        *expected = dstValue;

    return success;
}

#define atomic_compare_exchange_strong(object, expected, desired)   (sizeof(*(object)) == 1 ? atomic_compare_exchange8_((volatile atomic_schar*)(object), (int8_t*)(expected), (int8_t)(desired)) : \
                                                                    (sizeof(*(object)) == 2 ? atomic_compare_exchange16_((volatile atomic_short*)(object), (int16_t*)(expected), (int16_t)(desired)) : \
                                                                    (sizeof(*(object)) == 4 ? atomic_compare_exchange32_((volatile atomic_long *)(object), (int32_t*)(expected), (int32_t)(desired)) : \
                                                                     atomic_compare_exchange64_((volatile atomic_llong*)(object), (int64_t*)(expected), (int64_t)(desired)) )))

#define atomic_compare_exchange_weak    atomic_compare_exchange_strong

inline int8_t atomic_fetch_add8_(volatile atomic_schar* object, int8_t operand)
{
    int8_t expected = *object;
    int8_t desired;
    bool success;

    do
    {
        desired = expected + operand;
    } while ((success = atomic_compare_exchange8_(object, &expected, desired)),
        (success ? (void)0 : _mm_pause()),
        !success);

    return expected;
}

inline int16_t atomic_fetch_add16_(volatile atomic_short* object, int16_t operand)
{
    int16_t expected = *object;
    int16_t desired;
    bool success;

    do
    {
        desired = expected + operand;
    } while ((success = atomic_compare_exchange16_(object, &expected, desired)),
        (success ? (void)0 : _mm_pause()),
        !success);

    return expected;
}

inline int32_t atomic_fetch_add32_(volatile atomic_long* object, int32_t operand)
{
    int32_t expected = *object;
    int32_t desired;
    bool success;

    do
    {
        desired = expected + operand;
    } while ((success = atomic_compare_exchange32_(object, &expected, desired)),
        (success ? (void)0 : _mm_pause()),
        !success);

    return expected;
}

inline int64_t atomic_fetch_add64_(volatile atomic_llong* object, int64_t operand)
{
    int64_t expected = *object;
    int64_t desired;
    bool success;

    do
    {
        desired = expected + operand;
    } while ((success = atomic_compare_exchange64_(object, &expected, desired)),
        (success ? (void)0 : _mm_pause()),
        !success);

    return expected;
}

#define atomic_fetch_add(object, operand)   ((sizeof(*(object)) == 1) ? atomic_fetch_add8_((volatile atomic_schar*)(object), (int8_t)(operand)) : \
                                            ((sizeof(*(object)) == 2) ? atomic_fetch_add16_((volatile atomic_short*)(object), (int16_t)(operand)) : \
                                            ((sizeof(*(object)) == 4) ? atomic_fetch_add32_((volatile atomic_long*)(object), (int32_t)(operand)) : \
                                             atomic_fetch_add64_((volatile atomic_llong*)(object), (int64_t)(operand)) )))


inline int8_t atomic_fetch_sub8_(volatile atomic_schar* object, int8_t operand)
{
    int8_t expected = *object;
    int8_t desired;
    bool success;

    do
    {
        desired = expected - operand;
    } while ((success =atomic_compare_exchange8_(object, &expected, desired)),
        (success ? (void)0 : _mm_pause()),
        !success);

    return expected;
}

inline int16_t atomic_fetch_sub16_(volatile atomic_short* object, int16_t operand)
{
    int16_t expected = *object;
    int16_t desired;
    bool success;

    do
    {
        desired = expected - operand;
    } while ((success = atomic_compare_exchange16_(object, &expected, desired)),
        (success ? (void)0 : _mm_pause()),
        !success);

    return expected;
}

inline int32_t atomic_fetch_sub32_(volatile atomic_long* object, int32_t operand)
{
    int32_t expected = *object;
    int32_t desired;
    bool success;

    do
    {
        desired = expected - operand;
    } while ((success = atomic_compare_exchange32_(object, &expected, desired)),
        (success ? (void)0 : _mm_pause()),
        !success);

    return expected;
}

inline int64_t atomic_fetch_sub64_(volatile atomic_llong* object, int64_t operand)
{
    int64_t expected = *object;
    int64_t desired;
    bool success;

    do
    {
        desired = expected - operand;
    } while ((success = atomic_compare_exchange64_(object, &expected, desired)),
        (success ? (void)0 : _mm_pause()),
        !success);

    return expected;
}

#define atomic_fetch_sub(object, operand)   ((sizeof(*(object)) == 1) ? atomic_fetch_sub8_((volatile atomic_schar*)(object), (int8_t)(operand)) : \
                                            ((sizeof(*(object)) == 2) ? atomic_fetch_sub16_((volatile atomic_short*)(object), (int16_t)(operand)) : \
                                            ((sizeof(*(object)) == 4) ? atomic_fetch_sub32_((volatile atomic_long*)(object), (int32_t)(operand)) : \
                                             atomic_fetch_sub64_((volatile atomic_llong*)(object), (int64_t)(operand)) )))

inline int8_t atomic_fetch_and8_(volatile atomic_schar* object, int8_t operand)
{
    return _InterlockedAnd8(object, operand);
}

inline int16_t atomic_fetch_and16_(volatile atomic_short* object, int16_t operand)
{
    return _InterlockedAnd16(object, operand);
}

inline int32_t atomic_fetch_and32_(volatile atomic_long* object, int32_t operand)
{
    return _InterlockedAnd(object, operand);
}

inline int64_t atomic_fetch_and64_(volatile atomic_llong* object, int64_t operand)
{
    return _InterlockedAnd64(object, operand);
}

#define atomic_fetch_and(object, operand)   ((sizeof(*(object)) == 1) ? atomic_fetch_and8_((volatile atomic_schar*)(object), (int8_t)(operand)) : \
                                            ((sizeof(*(object)) == 2) ? atomic_fetch_and16_((volatile atomic_short*)(object), (int16_t)(operand)) : \
                                            ((sizeof(*(object)) == 4) ? atomic_fetch_and32_((volatile atomic_long*)(object), (int32_t)(operand)) : \
                                             atomic_fetch_and64_((volatile atomic_llong*)(object), (int64_t)(operand)) )))
#else
#include <stdatomic.h>
#endif

#define ATOM_BYTE atomic_uchar
#define ATOM_INT atomic_int
#define ATOM_POINTER atomic_uintptr_t
#define ATOM_SIZET atomic_size_t
#define ATOM_INIT(ref, v) atomic_init(ref, v)
#define ATOM_LOAD(ptr) atomic_load(ptr)
#define ATOM_STORE(ptr, v) atomic_store(ptr, v)
#define ATOM_CAS(ptr, oval, nval) atomic_compare_exchange_weak(ptr, &(oval), nval)
#define ATOM_CAS_POINTER(ptr, oval, nval) atomic_compare_exchange_weak(ptr, &(oval), nval)
#define ATOM_FINC(ptr) atomic_fetch_add(ptr, 1)
#define ATOM_FDEC(ptr) atomic_fetch_sub(ptr, 1)
#define ATOM_FADD(ptr,n) atomic_fetch_add(ptr, n)
#define ATOM_FSUB(ptr,n) atomic_fetch_sub(ptr, n)
#define ATOM_FAND(ptr,n) atomic_fetch_and(ptr, n)

#endif