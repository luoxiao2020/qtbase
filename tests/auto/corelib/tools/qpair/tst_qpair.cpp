/****************************************************************************
**
** Copyright (C) 2012 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com, author Marc Mutz <marc.mutz@kdab.com>
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtTest/QtTest>

#include <QPair>
#include <QSize>

class tst_QPair : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void pairOfReferences();
    void structuredBindings();
    void testConstexpr();
    void testConversions();
    void taskQTBUG_48780_pairContainingCArray();
    void testDeductionRules();
};

class C { C() {} char _[4]; };
class M { M() {} char _[4]; };
class P { char _[4]; };

QT_BEGIN_NAMESPACE
Q_DECLARE_TYPEINFO(M, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(P, Q_PRIMITIVE_TYPE);
QT_END_NAMESPACE

// avoid the comma:
typedef QPair<C,C> QPairCC;
typedef QPair<C,M> QPairCM;
typedef QPair<C,P> QPairCP;
typedef QPair<M,C> QPairMC;
typedef QPair<M,M> QPairMM;
typedef QPair<M,P> QPairMP;
typedef QPair<P,C> QPairPC;
typedef QPair<P,M> QPairPM;
typedef QPair<P,P> QPairPP;

static_assert( QTypeInfo<QPairCC>::isComplex);
static_assert( QTypeInfo<QPairCC>::isStatic );

static_assert( QTypeInfo<QPairCM>::isComplex);
static_assert( QTypeInfo<QPairCM>::isStatic );

static_assert( QTypeInfo<QPairCP>::isComplex);
static_assert( QTypeInfo<QPairCP>::isStatic );

static_assert( QTypeInfo<QPairMC>::isComplex);
static_assert( QTypeInfo<QPairMC>::isStatic );

static_assert( QTypeInfo<QPairMM>::isComplex);
static_assert(!QTypeInfo<QPairMM>::isStatic );

static_assert( QTypeInfo<QPairMP>::isComplex);
static_assert(!QTypeInfo<QPairMP>::isStatic );

static_assert( QTypeInfo<QPairPC>::isComplex);
static_assert( QTypeInfo<QPairPC>::isStatic );

static_assert( QTypeInfo<QPairPM>::isComplex);
static_assert(!QTypeInfo<QPairPM>::isStatic );

static_assert(!QTypeInfo<QPairPP>::isComplex);
static_assert(!QTypeInfo<QPairPP>::isStatic );

static_assert(!QTypeInfo<QPairPP>::isPointer);


void tst_QPair::pairOfReferences()
{
    int i = 0;
    QString s;

    QPair<int&, QString&> p(i, s);

    p.first = 1;
    QCOMPARE(i, 1);

    i = 2;
    QCOMPARE(p.first, 2);

    p.second = QLatin1String("Hello");
    QCOMPARE(s, QLatin1String("Hello"));

    s = QLatin1String("olleH");
    QCOMPARE(p.second, QLatin1String("olleH"));

    QPair<int&, QString&> q = p;
    q.first = 3;
    QCOMPARE(i, 3);
    QCOMPARE(p.first, 3);

    q.second = QLatin1String("World");
    QCOMPARE(s, QLatin1String("World"));
    QCOMPARE(p.second, QLatin1String("World"));
}

void tst_QPair::structuredBindings()
{
    using PV = QPair<int, QString>;
    using PR = QPair<int&, const QString&>;

    {
        PV pv = {42, "Hello"};
        PR pr = {pv.first, pv.second};

        auto [fv, sv] = pv;

        fv = 24;
        sv = "World";
        QCOMPARE(fv, 24);
        QCOMPARE(sv, "World");
        QCOMPARE(pv.first, 42);
        QCOMPARE(pv.second, "Hello");

        auto [fr, sr] = pr;

        fr = 2424;
        // sr = "World"; // const
        QCOMPARE(fr, 2424);
        QCOMPARE(pv.first, 2424);
    }

    {
        PV pv = {42, "Hello"};
        PR pr = {pv.first, pv.second};

        auto& [fv, sv] = pv;

        fv = 24;
        sv = "World";
        QCOMPARE(fv, 24);
        QCOMPARE(sv, "World");
        QCOMPARE(pv.first, 24);
        QCOMPARE(pv.second, "World");

        auto& [fr, sr] = pr;

        fr = 4242;
        //sr = "2World"; // const

        QCOMPARE(fr, 4242);
        QCOMPARE(pr.first, 4242);
        QCOMPARE(pv.first, 4242);
    }
}

void tst_QPair::testConstexpr()
{
    Q_CONSTEXPR QPair<int, double> pID = qMakePair(0, 0.0);
    Q_UNUSED(pID);

    Q_CONSTEXPR QPair<double, double> pDD  = qMakePair(0.0, 0.0);
    Q_CONSTEXPR QPair<double, double> pDD2 = qMakePair(0, 0.0);   // involes (rvalue) conversion ctor
    Q_CONSTEXPR bool equal = pDD2 == pDD;
    QVERIFY(equal);

    Q_CONSTEXPR QPair<QSize, int> pSI = qMakePair(QSize(4, 5), 6);
    Q_UNUSED(pSI);
}

void tst_QPair::testConversions()
{
    // construction from lvalue:
    {
        const QPair<int, double> rhs(42, 4.5);
        const QPair<int, int> pii = rhs;
        QCOMPARE(pii.first, 42);
        QCOMPARE(pii.second, 4);

        const QPair<int, float> pif = rhs;
        QCOMPARE(pif.first, 42);
        QCOMPARE(pif.second, 4.5f);
    }

    // assignment from lvalue:
    {
        const QPair<int, double> rhs(42, 4.5);
        QPair<int, int> pii;
        pii = rhs;
        QCOMPARE(pii.first, 42);
        QCOMPARE(pii.second, 4);

        QPair<int, float> pif;
        pif = rhs;
        QCOMPARE(pif.first, 42);
        QCOMPARE(pif.second, 4.5f);
    }

    // construction from rvalue:
    {
#define rhs qMakePair(42, 4.5)
        const QPair<int, int> pii = rhs;
        QCOMPARE(pii.first, 42);
        QCOMPARE(pii.second, 4);

        const QPair<int, float> pif = rhs;
        QCOMPARE(pif.first, 42);
        QCOMPARE(pif.second, 4.5f);
#undef rhs
    }

    // assignment from rvalue:
    {
#define rhs qMakePair(42, 4.5)
        QPair<int, int> pii;
        pii = rhs;
        QCOMPARE(pii.first, 42);
        QCOMPARE(pii.second, 4);

        QPair<int, float> pif;
        pif = rhs;
        QCOMPARE(pif.first, 42);
        QCOMPARE(pif.second, 4.5f);
#undef rhs
    }
}

void tst_QPair::taskQTBUG_48780_pairContainingCArray()
{
    // compile-only:
    QPair<int[2], int> pair;
    pair.first[0] = 0;
    pair.first[1] = 1;
    pair.second = 2;
    Q_UNUSED(pair);
}

void tst_QPair::testDeductionRules()
{
#if defined(__cpp_deduction_guides) && __cpp_deduction_guides >= 201907L
    QPair p1{1, 2};
    static_assert(std::is_same<decltype(p1)::first_type, decltype(1)>::value);
    static_assert(std::is_same<decltype(p1)::second_type, decltype(2)>::value);
    QCOMPARE(p1.first, 1);
    QCOMPARE(p1.second, 2);

    QPair p2{QString("string"), 2};
    static_assert(std::is_same<decltype(p2)::first_type, QString>::value);
    static_assert(std::is_same<decltype(p2)::second_type, decltype(2)>::value);
    QCOMPARE(p2.first, "string");
    QCOMPARE(p2.second, 2);

    QPair p3(p2);
    static_assert(std::is_same<decltype(p3)::first_type, decltype(p2)::first_type>::value);
    static_assert(std::is_same<decltype(p3)::second_type, decltype(p2)::second_type>::value);
    QCOMPARE(p3.first, "string");
    QCOMPARE(p3.second, 2);
#else
    QSKIP("Unsupported (requires C++20's CTAD for aliases)");
#endif
}

QTEST_APPLESS_MAIN(tst_QPair)
#include "tst_qpair.moc"
