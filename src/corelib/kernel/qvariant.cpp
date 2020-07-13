/****************************************************************************
**
** Copyright (C) 2020 The Qt Company Ltd.
** Copyright (C) 2018 Intel Corporation.
** Copyright (C) 2015 Olivier Goffart <ogoffart@woboq.com>
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtCore module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qvariant.h"
#include "qbitarray.h"
#include "qbytearray.h"
#include "qdatastream.h"
#include "qdebug.h"
#include "qmap.h"
#include "qdatetime.h"
#if QT_CONFIG(easingcurve)
#include "qeasingcurve.h"
#endif
#include "qlist.h"
#if QT_CONFIG(regularexpression)
#include "qregularexpression.h"
#endif
#include "qstring.h"
#include "qstringlist.h"
#include "qurl.h"
#include "qlocale.h"
#include "quuid.h"
#if QT_CONFIG(itemmodel)
#include "qabstractitemmodel.h"
#endif
#ifndef QT_BOOTSTRAPPED
#include "qcborarray.h"
#include "qcborcommon.h"
#include "qcbormap.h"
#include "qjsonvalue.h"
#include "qjsonobject.h"
#include "qjsonarray.h"
#include "qjsondocument.h"
#include "qbytearraylist.h"
#endif
#include "private/qvariant_p.h"
#include "private/qlocale_p.h"
#include "qmetatype_p.h"
#include <qmetaobject.h>

#ifndef QT_NO_GEOM_VARIANT
#include "qsize.h"
#include "qpoint.h"
#include "qrect.h"
#include "qline.h"
#endif

#include <cmath>
#include <float.h>
#include <cstring>

QT_BEGIN_NAMESPACE

namespace { // annonymous used to hide QVariant handlers

/*!
  \internal
 */
static qlonglong qMetaTypeNumber(const QVariant::Private *d)
{
    switch (d->typeId()) {
    case QMetaType::Int:
        return d->get<int>();
    case QMetaType::LongLong:
        return d->get<qlonglong>();
    case QMetaType::Char:
        return qlonglong(d->get<char>());
    case QMetaType::SChar:
        return qlonglong(d->get<signed char>());
    case QMetaType::Short:
        return qlonglong(d->get<short>());
    case QMetaType::Long:
        return qlonglong(d->get<long>());
    case QMetaType::Float:
        return qRound64(d->get<float>());
    case QMetaType::Double:
        return qRound64(d->get<double>());
#ifndef QT_BOOTSTRAPPED
    case QMetaType::QJsonValue:
        return d->get<QJsonValue>().toDouble();
    case QMetaType::QCborValue:
        return d->get<QCborValue>().toInteger();
#endif
    }
    Q_ASSERT(false);
    return 0;
}

static qulonglong qMetaTypeUNumber(const QVariant::Private *d)
{
    switch (d->typeId()) {
    case QMetaType::UInt:
        return d->get<unsigned int>();
    case QMetaType::ULongLong:
        return d->get<qulonglong>();
    case QMetaType::UChar:
        return d->get<unsigned char>();
    case QMetaType::UShort:
        return d->get<unsigned short>();
    case QMetaType::ULong:
        return d->get<unsigned long>();
    }
    Q_ASSERT(false);
    return 0;
}

static qlonglong qConvertToNumber(const QVariant::Private *d, bool *ok, bool allowStringToBool = false)
{
    *ok = true;

    switch (uint(d->typeId())) {
    case QMetaType::QString: {
        const QString &s = d->get<QString>();
        qlonglong l = s.toLongLong(ok);
        if (*ok)
            return l;
        if (allowStringToBool) {
            if (s == QLatin1String("false") || s == QLatin1String("0")) {
                *ok = true;
                return 0;
            }
            if (s == QLatin1String("true") || s == QLatin1String("1")) {
                *ok = true;
                return 1;
            }
        }
        return 0;
    }
    case QMetaType::QChar:
        return d->get<QChar>().unicode();
    case QMetaType::QByteArray:
        return d->get<QByteArray>().toLongLong(ok);
    case QMetaType::Bool:
        return qlonglong(d->get<bool>());
#ifndef QT_BOOTSTRAPPED
    case QMetaType::QCborValue:
        if (!d->get<QCborValue>().isInteger() && !d->get<QCborValue>().isDouble())
            break;
        return qMetaTypeNumber(d);
    case QMetaType::QJsonValue:
        if (!d->get<QJsonValue>().isDouble())
            break;
        Q_FALLTHROUGH();
#endif
    case QMetaType::Double:
    case QMetaType::Int:
    case QMetaType::Char:
    case QMetaType::SChar:
    case QMetaType::Short:
    case QMetaType::Long:
    case QMetaType::Float:
    case QMetaType::LongLong:
        return qMetaTypeNumber(d);
    case QMetaType::ULongLong:
    case QMetaType::UInt:
    case QMetaType::UChar:
    case QMetaType::UShort:
    case QMetaType::ULong:

        return qlonglong(qMetaTypeUNumber(d));
    }

    QMetaType typeInfo = d->type();
    if (typeInfo.flags() & QMetaType::IsEnumeration
        || d->typeId() == QMetaType::QCborSimpleType) {
        switch (typeInfo.sizeOf()) {
        case 1:
            return d->get<signed char>();
        case 2:
            return d->get<short>();
        case 4:
            return d->get<int>();
        case 8:
            return d->get<qlonglong>();
        }
    }

    *ok = false;
    return Q_INT64_C(0);
}

static qreal qConvertToRealNumber(const QVariant::Private *d, bool *ok)
{
    *ok = true;
    switch (uint(d->typeId())) {
    case QMetaType::QString:
        return d->get<QString>().toDouble(ok);
    case QMetaType::Double:
        return qreal(d->get<double>());
    case QMetaType::Float:
        return qreal(d->get<float>());
    case QMetaType::ULongLong:
    case QMetaType::UInt:
    case QMetaType::UChar:
    case QMetaType::UShort:
    case QMetaType::ULong:
        return qreal(qMetaTypeUNumber(d));
#ifndef QT_BOOTSTRAPPED
    case QMetaType::QCborValue:
        return d->get<QCborValue>().toDouble();
    case QMetaType::QJsonValue:
        return d->get<QJsonValue>().toDouble();
#endif
    default:
        // includes enum conversion as well as invalid types
        return qreal(qConvertToNumber(d, ok));
    }
}

/*!
 \internal

 Converts \a d to type \a t, which is placed in \a result.
 */
static bool convert(const QVariant::Private *d, int t, void *result)
{
    Q_ASSERT(d->typeId() != t);
    Q_ASSERT(result);

    bool ok = true;

    switch (uint(t)) {
    case QMetaType::Nullptr:
        *static_cast<std::nullptr_t *>(result) = nullptr;
        if (QMetaType::typeFlags(t) & (QMetaType::PointerToGadget | QMetaType::PointerToQObject)
            || d->typeId() == QMetaType::VoidStar) {
            if (d->get<const void *>() == nullptr)
                break;
        }
#ifndef QT_BOOTSTRAPPED
        if (d->typeId() == QMetaType::QCborValue && d->get<QCborValue>().isNull())
            break;
#endif
        return false;

#ifndef QT_BOOTSTRAPPED
    case QMetaType::QJsonValue:
        switch (d->typeId()) {
        case QMetaType::Nullptr:
            *static_cast<QJsonValue *>(result) = QJsonValue(QJsonValue::Null);
            break;
        case QMetaType::Bool:
            *static_cast<QJsonValue *>(result) = QJsonValue(d->get<bool>());
            break;
        case QMetaType::Int:
        case QMetaType::UInt:
        case QMetaType::Double:
        case QMetaType::Float:
        case QMetaType::ULong:
        case QMetaType::Long:
        case QMetaType::LongLong:
        case QMetaType::ULongLong:
        case QMetaType::UShort:
        case QMetaType::UChar:
        case QMetaType::Char:
        case QMetaType::SChar:
        case QMetaType::Short:
            *static_cast<QJsonValue *>(result) = QJsonValue(qConvertToRealNumber(d, &ok));
            Q_ASSERT(ok);
            break;
        case QMetaType::QString:
            *static_cast<QJsonValue *>(result) = QJsonValue(d->get<QString>());
            break;
        case QMetaType::QStringList:
            *static_cast<QJsonValue *>(result) = QJsonValue(QJsonArray::fromStringList(d->get<QStringList>()));
            break;
        case QMetaType::QVariantList:
            *static_cast<QJsonValue *>(result) = QJsonValue(QJsonArray::fromVariantList(d->get<QVariantList>()));
            break;
        case QMetaType::QVariantMap:
            *static_cast<QJsonValue *>(result) = QJsonValue(QJsonObject::fromVariantMap(d->get<QVariantMap>()));
            break;
        case QMetaType::QVariantHash:
            *static_cast<QJsonValue *>(result) = QJsonValue(QJsonObject::fromVariantHash(d->get<QVariantHash>()));
            break;
        case QMetaType::QJsonObject:
            *static_cast<QJsonValue *>(result) = d->get<QJsonObject>();
            break;
        case QMetaType::QJsonArray:
            *static_cast<QJsonValue *>(result) = d->get<QJsonArray>();
            break;
        case QMetaType::QJsonDocument: {
            QJsonDocument doc = d->get<QJsonDocument>();
            *static_cast<QJsonValue *>(result) = doc.isArray() ? QJsonValue(doc.array()) : QJsonValue(doc.object());
            break;
        }
        case QMetaType::QCborValue:
            *static_cast<QJsonValue *>(result) = d->get<QCborValue>().toJsonValue();
            break;
        case QMetaType::QCborMap:
            *static_cast<QJsonValue *>(result) = d->get<QCborMap>().toJsonObject();
            break;
        case QMetaType::QCborArray:
            *static_cast<QJsonValue *>(result) = d->get<QCborArray>().toJsonArray();
            break;
        default:
            *static_cast<QJsonValue *>(result) = QJsonValue(QJsonValue::Undefined);
            return false;
        }
        break;
   case QMetaType::QCborSimpleType:
        if (d->typeId() == QMetaType::QCborValue && d->get<QCborValue>().isSimpleType()) {
            *static_cast<QCborSimpleType *>(result) = d->get<QCborValue>().toSimpleType();
            break;
        }
        return false;
    case QMetaType::QCborValue:
        switch (d->typeId()) {
        case QMetaType::Nullptr:
            *static_cast<QCborValue *>(result) = QCborValue(QCborValue::Null);
            break;
        case QMetaType::Bool:
            *static_cast<QCborValue *>(result) = QCborValue(d->get<bool>());
            break;
        case QMetaType::Int:
        case QMetaType::UInt:
        case QMetaType::ULong:
        case QMetaType::Long:
        case QMetaType::LongLong:
        case QMetaType::ULongLong:
        case QMetaType::UShort:
        case QMetaType::UChar:
        case QMetaType::Char:
        case QMetaType::SChar:
        case QMetaType::Short:
            *static_cast<QCborValue *>(result) = QCborValue(qConvertToNumber(d, &ok));
            Q_ASSERT(ok);
            break;
        case QMetaType::Double:
        case QMetaType::Float:
            *static_cast<QCborValue *>(result) = QCborValue(qConvertToRealNumber(d, &ok));
            Q_ASSERT(ok);
            break;
        case QMetaType::QString:
            *static_cast<QCborValue *>(result) = d->get<QString>();
            break;
        case QMetaType::QStringList:
            *static_cast<QCborValue *>(result) = QCborArray::fromStringList(d->get<QStringList>());
            break;
        case QMetaType::QByteArray:
            *static_cast<QCborValue *>(result) = d->get<QByteArray>();
            break;
        case QMetaType::QDate:
            *static_cast<QCborValue *>(result) = QCborValue(d->get<QDate>().startOfDay());
            break;
        case QMetaType::QDateTime:
            *static_cast<QCborValue *>(result) = QCborValue(d->get<QDateTime>());
            break;
        case QMetaType::QUrl:
            *static_cast<QCborValue *>(result) = QCborValue(d->get<QUrl>());
            break;
        case QMetaType::QJsonValue:
            *static_cast<QCborValue *>(result) = QCborValue::fromJsonValue(d->get<QJsonValue>());
            break;
        case QMetaType::QJsonObject:
            *static_cast<QCborValue *>(result) = QCborMap::fromJsonObject(d->get<QJsonObject>());
            break;
        case QMetaType::QJsonArray:
            *static_cast<QCborValue *>(result) = QCborArray::fromJsonArray(d->get<QJsonArray>());
            break;
        case QMetaType::QJsonDocument: {
            QJsonDocument doc = d->get<QJsonDocument>();
            if (doc.isArray())
                *static_cast<QCborValue *>(result) = QCborArray::fromJsonArray(doc.array());
            else
                *static_cast<QCborValue *>(result) = QCborMap::fromJsonObject(doc.object());
            break;
        }
        case QMetaType::QCborSimpleType:
            *static_cast<QCborValue *>(result) = d->get<QCborSimpleType>();
            break;
        case QMetaType::QCborMap:
            *static_cast<QCborValue *>(result) = d->get<QCborMap>();
            break;
        case QMetaType::QCborArray:
            *static_cast<QCborValue *>(result) = d->get<QCborArray>();
            break;
        default:
            *static_cast<QCborValue *>(result) = {};
            return false;
        }
        break;
#endif

    default:
        return false;
    }
    return true;
}

// the type of d has already been set, but other field are not set
static void customConstruct(QVariant::Private *d, const void *copy)
{
    const QMetaType type = d->type();
    const uint size = type.sizeOf();
    if (!size) {
        *d = QVariant::Private();
        return;
    }

    if (QVariant::Private::canUseInternalSpace(size)) {
        type.construct(&d->data, copy);
        d->is_shared = false;
    } else {
        d->data.shared = QVariant::PrivateShared::create(type);
        type.construct(d->data.shared->data(), copy);
        d->is_shared = true;
    }
    // need to check for nullptr_t here, as this can get called by fromValue(nullptr). fromValue() uses
    // std::addressof(value) which in this case returns the address of the nullptr object.
    d->is_null = !copy || type == QMetaType::fromType<std::nullptr_t>();
}

static void customClear(QVariant::Private *d)
{
    if (!d->is_shared) {
        d->type().destruct(&d->data);
    } else {
        d->type().destruct(d->data.shared->data());
        QVariant::PrivateShared::free(d->data.shared);
    }
}


} // annonymous used to hide QVariant handlers

/*!
    \class QVariant
    \inmodule QtCore
    \brief The QVariant class acts like a union for the most common Qt data types.

    \ingroup objectmodel
    \ingroup shared


    Because C++ forbids unions from including types that have
    non-default constructors or destructors, most interesting Qt
    classes cannot be used in unions. Without QVariant, this would be
    a problem for QObject::property() and for database work, etc.

    A QVariant object holds a single value of a single type() at a
    time. (Some type()s are multi-valued, for example a string list.)
    You can find out what type, T, the variant holds, convert it to a
    different type using convert(), get its value using one of the
    toT() functions (e.g., toSize()) and check whether the type can
    be converted to a particular type using canConvert().

    The methods named toT() (e.g., toInt(), toString()) are const. If
    you ask for the stored type, they return a copy of the stored
    object. If you ask for a type that can be generated from the
    stored type, toT() copies and converts and leaves the object
    itself unchanged. If you ask for a type that cannot be generated
    from the stored type, the result depends on the type; see the
    function documentation for details.

    Here is some example code to demonstrate the use of QVariant:

    \snippet code/src_corelib_kernel_qvariant.cpp 0

    You can even store QList<QVariant> and QMap<QString, QVariant>
    values in a variant, so you can easily construct arbitrarily
    complex data structures of arbitrary types. This is very powerful
    and versatile, but may prove less memory and speed efficient than
    storing specific types in standard data structures.

    QVariant also supports the notion of null values, where you can
    have a defined type with no value set. However, note that QVariant
    types can only be cast when they have had a value set.

    \snippet code/src_corelib_kernel_qvariant.cpp 1

    QVariant can be extended to support other types than those
    mentioned in the \l Type enum. See \l{Creating Custom Qt Types}{Creating Custom Qt Types}
    for details.

    \section1 A Note on GUI Types

    Because QVariant is part of the Qt Core module, it cannot provide
    conversion functions to data types defined in Qt GUI, such as
    QColor, QImage, and QPixmap. In other words, there is no \c
    toColor() function. Instead, you can use the QVariant::value() or
    the qvariant_cast() template function. For example:

    \snippet code/src_corelib_kernel_qvariant.cpp 2

    The inverse conversion (e.g., from QColor to QVariant) is
    automatic for all data types supported by QVariant, including
    GUI-related types:

    \snippet code/src_corelib_kernel_qvariant.cpp 3

    \section1 Using canConvert() and convert() Consecutively

    When using canConvert() and convert() consecutively, it is possible for
    canConvert() to return true, but convert() to return false. This
    is typically because canConvert() only reports the general ability of
    QVariant to convert between types given suitable data; it is still
    possible to supply data which cannot actually be converted.

    For example, canConvert(Int) would return true when called on a variant
    containing a string because, in principle, QVariant is able to convert
    strings of numbers to integers.
    However, if the string contains non-numeric characters, it cannot be
    converted to an integer, and any attempt to convert it will fail.
    Hence, it is important to have both functions return true for a
    successful conversion.

    \sa QMetaType
*/

/*!
    \obsolete Use QMetaType::Type instead
    \enum QVariant::Type

    This enum type defines the types of variable that a QVariant can
    contain.

    \value Invalid  no type
    \value BitArray  a QBitArray
    \value Bitmap  a QBitmap
    \value Bool  a bool
    \value Brush  a QBrush
    \value ByteArray  a QByteArray
    \value Char  a QChar
    \value Color  a QColor
    \value Cursor  a QCursor
    \value Date  a QDate
    \value DateTime  a QDateTime
    \value Double  a double
    \value EasingCurve a QEasingCurve
    \value Uuid a QUuid
    \value ModelIndex a QModelIndex
    \value PersistentModelIndex a QPersistentModelIndex (since 5.5)
    \value Font  a QFont
    \value Hash a QVariantHash
    \value Icon  a QIcon
    \value Image  a QImage
    \value Int  an int
    \value KeySequence  a QKeySequence
    \value Line  a QLine
    \value LineF  a QLineF
    \value List  a QVariantList
    \value Locale  a QLocale
    \value LongLong a \l qlonglong
    \value Map  a QVariantMap
    \value Transform  a QTransform
    \value Matrix4x4  a QMatrix4x4
    \value Palette  a QPalette
    \value Pen  a QPen
    \value Pixmap  a QPixmap
    \value Point  a QPoint
    \value PointF  a QPointF
    \value Polygon a QPolygon
    \value PolygonF a QPolygonF
    \value Quaternion  a QQuaternion
    \value Rect  a QRect
    \value RectF  a QRectF
    \value RegularExpression  a QRegularExpression
    \value Region  a QRegion
    \value Size  a QSize
    \value SizeF  a QSizeF
    \value SizePolicy  a QSizePolicy
    \value String  a QString
    \value StringList  a QStringList
    \value TextFormat  a QTextFormat
    \value TextLength  a QTextLength
    \value Time  a QTime
    \value UInt  a \l uint
    \value ULongLong a \l qulonglong
    \value Url  a QUrl
    \value Vector2D  a QVector2D
    \value Vector3D  a QVector3D
    \value Vector4D  a QVector4D

    \value UserType Base value for user-defined types.

    \omitvalue LastGuiType
    \omitvalue LastCoreType
    \omitvalue LastType
*/

/*!
    \fn QVariant::QVariant(QVariant &&other)

    Move-constructs a QVariant instance, making it point at the same
    object that \a other was pointing to.

    \since 5.2
*/

/*!
    \fn QVariant &QVariant::operator=(QVariant &&other)

    Move-assigns \a other to this QVariant instance.

    \since 5.2
*/

/*!
    \fn QVariant::QVariant()

    Constructs an invalid variant.
*/


/*!
    \fn QVariant::QVariant(int typeId, const void *copy)

    Constructs variant of type \a typeId, and initializes with
    \a copy if \a copy is not \nullptr.

    Note that you have to pass the address of the variable you want stored.

    Usually, you never have to use this constructor, use QVariant::fromValue()
    instead to construct variants from the pointer types represented by
    \c QMetaType::VoidStar, and \c QMetaType::QObjectStar.

    \sa QVariant::fromValue(), QMetaType::Type
*/

/*!
    \fn QVariant::QVariant(Type type)

    Constructs an uninitialized variant of type \a type. This will create a
    variant in a special null state that if accessed will return a default
    constructed value of the \a type.

    \sa isNull()
*/



/*!
    \fn QVariant::create(int type, const void *copy)

    \internal

    Constructs a variant private of type \a type, and initializes with \a copy if
    \a copy is not \nullptr.
*/

void QVariant::create(int type, const void *copy)
{
    d = Private(QMetaType(type));
    customConstruct(&d, copy);
}

/*!
    \fn QVariant::~QVariant()

    Destroys the QVariant and the contained object.

    Note that subclasses that reimplement clear() should reimplement
    the destructor to call clear(). This destructor calls clear(), but
    because it is the destructor, QVariant::clear() is called rather
    than a subclass's clear().
*/

QVariant::~QVariant()
{
    if ((d.is_shared && !d.data.shared->ref.deref()) || (!d.is_shared))
        customClear(&d);
}

/*!
  \fn QVariant::QVariant(const QVariant &p)

    Constructs a copy of the variant, \a p, passed as the argument to
    this constructor.
*/

QVariant::QVariant(const QVariant &p)
    : d(p.d)
{
    if (d.is_shared) {
        d.data.shared->ref.ref();
        return;
    }
    QMetaType t = d.type();
    if (t.isValid())
        t.construct(&d, p.constData());
}

#ifndef QT_NO_DATASTREAM
/*!
    Reads the variant from the data stream, \a s.
*/
QVariant::QVariant(QDataStream &s)
{
    d.is_null = true;
    s >> *this;
}
#endif //QT_NO_DATASTREAM

/*!
  \fn QVariant::QVariant(const QString &val)

    Constructs a new variant with a string value, \a val.
*/

/*!
  \fn QVariant::QVariant(QLatin1String val)

    Constructs a new variant with a string value, \a val.
*/

/*!
  \fn QVariant::QVariant(const char *val)

    Constructs a new variant with a string value of \a val.
    The variant creates a deep copy of \a val into a QString assuming
    UTF-8 encoding on the input \a val.

    Note that \a val is converted to a QString for storing in the
    variant and QVariant::userType() will return QMetaType::QString for
    the variant.

    You can disable this operator by defining \c
    QT_NO_CAST_FROM_ASCII when you compile your applications.
*/

/*!
  \fn QVariant::QVariant(const QStringList &val)

    Constructs a new variant with a string list value, \a val.
*/

/*!
  \fn QVariant::QVariant(const QMap<QString, QVariant> &val)

    Constructs a new variant with a map of \l {QVariant}s, \a val.
*/

/*!
  \fn QVariant::QVariant(const QHash<QString, QVariant> &val)

    Constructs a new variant with a hash of \l {QVariant}s, \a val.
*/

/*!
  \fn QVariant::QVariant(QDate val)

    Constructs a new variant with a date value, \a val.
*/

/*!
  \fn QVariant::QVariant(QTime val)

    Constructs a new variant with a time value, \a val.
*/

/*!
  \fn QVariant::QVariant(const QDateTime &val)

    Constructs a new variant with a date/time value, \a val.
*/

/*!
    \since 4.7
  \fn QVariant::QVariant(const QEasingCurve &val)

    Constructs a new variant with an easing curve value, \a val.
*/

/*!
    \since 5.0
    \fn QVariant::QVariant(const QUuid &val)

    Constructs a new variant with an uuid value, \a val.
*/

/*!
    \since 5.0
    \fn QVariant::QVariant(const QModelIndex &val)

    Constructs a new variant with a QModelIndex value, \a val.
*/

/*!
    \since 5.5
    \fn QVariant::QVariant(const QPersistentModelIndex &val)

    Constructs a new variant with a QPersistentModelIndex value, \a val.
*/

/*!
    \since 5.0
    \fn QVariant::QVariant(const QJsonValue &val)

    Constructs a new variant with a json value, \a val.
*/

/*!
    \since 5.0
    \fn QVariant::QVariant(const QJsonObject &val)

    Constructs a new variant with a json object value, \a val.
*/

/*!
    \since 5.0
    \fn QVariant::QVariant(const QJsonArray &val)

    Constructs a new variant with a json array value, \a val.
*/

/*!
    \since 5.0
    \fn QVariant::QVariant(const QJsonDocument &val)

    Constructs a new variant with a json document value, \a val.
*/

/*!
  \fn QVariant::QVariant(const QByteArray &val)

    Constructs a new variant with a bytearray value, \a val.
*/

/*!
  \fn QVariant::QVariant(const QBitArray &val)

    Constructs a new variant with a bitarray value, \a val.
*/

/*!
  \fn QVariant::QVariant(const QPoint &val)

  Constructs a new variant with a point value of \a val.
 */

/*!
  \fn QVariant::QVariant(const QPointF &val)

  Constructs a new variant with a point value of \a val.
 */

/*!
  \fn QVariant::QVariant(const QRectF &val)

  Constructs a new variant with a rect value of \a val.
 */

/*!
  \fn QVariant::QVariant(const QLineF &val)

  Constructs a new variant with a line value of \a val.
 */

/*!
  \fn QVariant::QVariant(const QLine &val)

  Constructs a new variant with a line value of \a val.
 */

/*!
  \fn QVariant::QVariant(const QRect &val)

  Constructs a new variant with a rect value of \a val.
 */

/*!
  \fn QVariant::QVariant(const QSize &val)

  Constructs a new variant with a size value of \a val.
 */

/*!
  \fn QVariant::QVariant(const QSizeF &val)

  Constructs a new variant with a size value of \a val.
 */

/*!
  \fn QVariant::QVariant(const QUrl &val)

  Constructs a new variant with a url value of \a val.
 */

/*!
  \fn QVariant::QVariant(int val)

    Constructs a new variant with an integer value, \a val.
*/

/*!
  \fn QVariant::QVariant(uint val)

    Constructs a new variant with an unsigned integer value, \a val.
*/

/*!
  \fn QVariant::QVariant(qlonglong val)

    Constructs a new variant with a long long integer value, \a val.
*/

/*!
  \fn QVariant::QVariant(qulonglong val)

    Constructs a new variant with an unsigned long long integer value, \a val.
*/


/*!
  \fn QVariant::QVariant(bool val)

    Constructs a new variant with a boolean value, \a val.
*/

/*!
  \fn QVariant::QVariant(double val)

    Constructs a new variant with a floating point value, \a val.
*/

/*!
  \fn QVariant::QVariant(float val)

    Constructs a new variant with a floating point value, \a val.
    \since 4.6
*/

/*!
    \fn QVariant::QVariant(const QList<QVariant> &val)

    Constructs a new variant with a list value, \a val.
*/

/*!
  \fn QVariant::QVariant(QChar c)

  Constructs a new variant with a char value, \a c.
*/

/*!
  \fn QVariant::QVariant(const QLocale &l)

  Constructs a new variant with a locale value, \a l.
*/

/*!
  \fn QVariant::QVariant(const QRegularExpression &re)

  \since 5.0

  Constructs a new variant with the regular expression value \a re.
*/

QVariant::QVariant(Type type)
{ create(type, nullptr); }

/*!
    \internal
 */
QVariant::QVariant(QMetaType type, const void *copy) : d(type)
{
    customConstruct(&d, copy);
}

QVariant::QVariant(int val)
    : d(Int)
{ d.set(val); }
QVariant::QVariant(uint val)
    : d(UInt)
{ d.set(val); }
QVariant::QVariant(qlonglong val)
    : d(LongLong)
{ d.set(val); }
QVariant::QVariant(qulonglong val)
    : d(ULongLong)
{ d.set(val); }
QVariant::QVariant(bool val)
    : d(Bool)
{ d.set(val); }
QVariant::QVariant(double val)
    : d(Double)
{ d.set(val); }
QVariant::QVariant(float val)
    : d(QMetaType::Float)
{ d.set(val); }

QVariant::QVariant(const QByteArray &val)
    : d(ByteArray)
{ v_construct<QByteArray>(&d, val); }
QVariant::QVariant(const QBitArray &val)
    : d(BitArray)
{ v_construct<QBitArray>(&d, val);  }
QVariant::QVariant(const QString &val)
    : d(String)
{ v_construct<QString>(&d, val);  }
QVariant::QVariant(QChar val)
    : d(Char)
{ v_construct<QChar>(&d, val);  }
QVariant::QVariant(QLatin1String val)
    : d(String)
{ v_construct<QString>(&d, val); }
QVariant::QVariant(const QStringList &val)
    : d(StringList)
{ v_construct<QStringList>(&d, val); }

QVariant::QVariant(QDate val)
    : d(Date)
{ v_construct<QDate>(&d, val); }
QVariant::QVariant(QTime val)
    : d(Time)
{ v_construct<QTime>(&d, val); }
QVariant::QVariant(const QDateTime &val)
    : d(DateTime)
{ v_construct<QDateTime>(&d, val); }
#if QT_CONFIG(easingcurve)
QVariant::QVariant(const QEasingCurve &val)
    : d(EasingCurve)
{ v_construct<QEasingCurve>(&d, val); }
#endif
QVariant::QVariant(const QList<QVariant> &list)
    : d(List)
{ v_construct<QVariantList>(&d, list); }
QVariant::QVariant(const QMap<QString, QVariant> &map)
    : d(Map)
{ v_construct<QVariantMap>(&d, map); }
QVariant::QVariant(const QHash<QString, QVariant> &hash)
    : d(Hash)
{ v_construct<QVariantHash>(&d, hash); }
#ifndef QT_NO_GEOM_VARIANT
QVariant::QVariant(const QPoint &pt)
    : d(Point)
{ v_construct<QPoint>(&d, pt); }
QVariant::QVariant(const QPointF &pt)
    : d(PointF)
{ v_construct<QPointF>(&d, pt); }
QVariant::QVariant(const QRectF &r)
    : d(RectF)
{ v_construct<QRectF>(&d, r); }
QVariant::QVariant(const QLineF &l)
    : d(LineF)
{ v_construct<QLineF>(&d, l); }
QVariant::QVariant(const QLine &l)
    : d(Line)
{ v_construct<QLine>(&d, l); }
QVariant::QVariant(const QRect &r)
    : d(Rect)
{ v_construct<QRect>(&d, r); }
QVariant::QVariant(const QSize &s)
    : d(Size)
{ v_construct<QSize>(&d, s); }
QVariant::QVariant(const QSizeF &s)
    : d(SizeF)
{ v_construct<QSizeF>(&d, s); }
#endif
#ifndef QT_BOOTSTRAPPED
QVariant::QVariant(const QUrl &u)
    : d(Url)
{ v_construct<QUrl>(&d, u); }
#endif
QVariant::QVariant(const QLocale &l)
    : d(Locale)
{ v_construct<QLocale>(&d, l); }
#if QT_CONFIG(regularexpression)
QVariant::QVariant(const QRegularExpression &re)
    : d(RegularExpression)
{ v_construct<QRegularExpression>(&d, re); }
#endif // QT_CONFIG(regularexpression)
QVariant::QVariant(const QUuid &uuid)
    : d(Uuid)
{ v_construct<QUuid>(&d, uuid); }
#ifndef QT_BOOTSTRAPPED
QVariant::QVariant(const QJsonValue &jsonValue)
    : d(QMetaType::QJsonValue)
{ v_construct<QJsonValue>(&d, jsonValue); }
QVariant::QVariant(const QJsonObject &jsonObject)
    : d(QMetaType::QJsonObject)
{ v_construct<QJsonObject>(&d, jsonObject); }
QVariant::QVariant(const QJsonArray &jsonArray)
    : d(QMetaType::QJsonArray)
{ v_construct<QJsonArray>(&d, jsonArray); }
QVariant::QVariant(const QJsonDocument &jsonDocument)
    : d(QMetaType::QJsonDocument)
{ v_construct<QJsonDocument>(&d, jsonDocument); }
#endif // QT_BOOTSTRAPPED
#if QT_CONFIG(itemmodel)
QVariant::QVariant(const QModelIndex &modelIndex)
    : d(ModelIndex)
{ v_construct<QModelIndex>(&d, modelIndex); }
QVariant::QVariant(const QPersistentModelIndex &modelIndex)
    : d(PersistentModelIndex)
{ v_construct<QPersistentModelIndex>(&d, modelIndex); }
#endif

/*!
    Returns the storage type of the value stored in the variant.
    Although this function is declared as returning QVariant::Type,
    the return value should be interpreted as QMetaType::Type. In
    particular, QVariant::UserType is returned here only if the value
    is equal or greater than QMetaType::User.

    Note that return values in the ranges QVariant::Char through
    QVariant::RegExp and QVariant::Font through QVariant::Transform
    correspond to the values in the ranges QMetaType::QChar through
    QMetaType::QRegularExpression and QMetaType::QFont through QMetaType::QQuaternion.

    Pay particular attention when working with char and QChar
    variants.  Note that there is no QVariant constructor specifically
    for type char, but there is one for QChar. For a variant of type
    QChar, this function returns QVariant::Char, which is the same as
    QMetaType::QChar, but for a variant of type \c char, this function
    returns QMetaType::Char, which is \e not the same as
    QVariant::Char.

    Also note that the types \c void*, \c long, \c short, \c unsigned
    \c long, \c unsigned \c short, \c unsigned \c char, \c float, \c
    QObject*, and \c QWidget* are represented in QMetaType::Type but
    not in QVariant::Type, and they can be returned by this function.
    However, they are considered to be user defined types when tested
    against QVariant::Type.

    To test whether an instance of QVariant contains a data type that
    is compatible with the data type you are interested in, use
    canConvert().

    \sa userType(), metaType()
*/
QVariant::Type QVariant::type() const
{
    int type = d.typeId();
    return type >= QMetaType::User ? UserType : static_cast<Type>(type);
}
/*!
    Returns the storage type of the value stored in the variant. For
    non-user types, this is the same as type().

    \sa type(), metaType()
*/
int QVariant::userType() const
{
    return d.typeId();
}

/*!
    \since 6.0

    Returns the QMetaType of the value stored in the variant.
*/
QMetaType QVariant::metaType() const
{
    return d.type();
}

/*!
    Assigns the value of the variant \a variant to this variant.
*/
QVariant& QVariant::operator=(const QVariant &variant)
{
    if (this == &variant)
        return *this;

    clear();
    if (variant.d.is_shared) {
        variant.d.data.shared->ref.ref();
        d = variant.d;
    } else {
        d = variant.d;
        QMetaType t = d.type();
        if (t.isValid())
            t.construct(&d, variant.constData());
    }

    return *this;
}

/*!
    \fn void QVariant::swap(QVariant &other)
    \since 4.8

    Swaps variant \a other with this variant. This operation is very
    fast and never fails.
*/

/*!
    \fn void QVariant::detach()

    \internal
*/

void QVariant::detach()
{
    if (!d.is_shared || d.data.shared->ref.loadRelaxed() == 1)
        return;

    Private dd(d.type());
    customConstruct(&dd, constData());
    if (!d.data.shared->ref.deref())
        customClear(&d);
    d.data.shared = dd.data.shared;
}

/*!
    \fn bool QVariant::isDetached() const

    \internal
*/

/*!
    Returns the name of the type stored in the variant. The returned
    strings describe the C++ datatype used to store the data: for
    example, "QFont", "QString", or "QVariantList". An Invalid
    variant returns 0.
*/
const char *QVariant::typeName() const
{
    // Cannot use d.type().name because we must return a char*
    return QMetaType::typeName(d.typeId());
}

/*!
    Convert this variant to type QMetaType::UnknownType and free up any resources
    used.
*/
void QVariant::clear()
{
    if ((d.is_shared && !d.data.shared->ref.deref()) || (!d.is_shared))
        customClear(&d);
    d = {};
}

/*!
    Converts the int representation of the storage type, \a typeId, to
    its string representation.

    Returns \nullptr if the type is QMetaType::UnknownType or doesn't exist.
*/
const char *QVariant::typeToName(int typeId)
{
    return QMetaType::typeName(typeId);
}


/*!
    Converts the string representation of the storage type given in \a
    name, to its enum representation.

    If the string representation cannot be converted to any enum
    representation, the variant is set to \c Invalid.
*/
QVariant::Type QVariant::nameToType(const char *name)
{
    int metaType = QMetaType::type(name);
    return metaType <= int(UserType) ? QVariant::Type(metaType) : UserType;
}

#ifndef QT_NO_DATASTREAM
enum { MapFromThreeCount = 36 };
static const ushort mapIdFromQt3ToCurrent[MapFromThreeCount] =
{
    QMetaType::UnknownType,
    QMetaType::QVariantMap,
    QMetaType::QVariantList,
    QMetaType::QString,
    QMetaType::QStringList,
    QMetaType::QFont,
    QMetaType::QPixmap,
    QMetaType::QBrush,
    QMetaType::QRect,
    QMetaType::QSize,
    QMetaType::QColor,
    QMetaType::QPalette,
    0, // ColorGroup
    QMetaType::QIcon,
    QMetaType::QPoint,
    QMetaType::QImage,
    QMetaType::Int,
    QMetaType::UInt,
    QMetaType::Bool,
    QMetaType::Double,
    0, // Buggy ByteArray, QByteArray never had id == 20
    QMetaType::QPolygon,
    QMetaType::QRegion,
    QMetaType::QBitmap,
    QMetaType::QCursor,
    QMetaType::QSizePolicy,
    QMetaType::QDate,
    QMetaType::QTime,
    QMetaType::QDateTime,
    QMetaType::QByteArray,
    QMetaType::QBitArray,
#if QT_CONFIG(shortcut)
    QMetaType::QKeySequence,
#else
    0, // QKeySequence
#endif
    QMetaType::QPen,
    QMetaType::LongLong,
    QMetaType::ULongLong,
#if QT_CONFIG(easingcurve)
    QMetaType::QEasingCurve
#endif
};

/*!
    Internal function for loading a variant from stream \a s. Use the
    stream operators instead.

    \internal
*/
void QVariant::load(QDataStream &s)
{
    clear();

    quint32 typeId;
    s >> typeId;
    if (s.version() < QDataStream::Qt_4_0) {
        if (typeId >= MapFromThreeCount)
            return;
        typeId = mapIdFromQt3ToCurrent[typeId];
    } else if (s.version() < QDataStream::Qt_5_0) {
        if (typeId == 127 /* QVariant::UserType */) {
            typeId = QMetaType::User;
        } else if (typeId >= 128 && typeId != QVariant::UserType) {
            // In Qt4 id == 128 was FirstExtCoreType. In Qt5 ExtCoreTypes set was merged to CoreTypes
            // by moving all ids down by 97.
            typeId -= 97;
        } else if (typeId == 75 /* QSizePolicy */) {
            typeId = QMetaType::QSizePolicy;
        } else if (typeId > 75 && typeId <= 86) {
            // and as a result these types received lower ids too
            // QKeySequence QPen QTextLength QTextFormat QTransform QMatrix4x4 QVector2D QVector3D QVector4D QQuaternion
            typeId -=1;
        }
    }

    qint8 is_null = false;
    if (s.version() >= QDataStream::Qt_4_2)
        s >> is_null;
    if (typeId == 27) {
        // used to be QRegExp in Qt 4/5
        typeId = QMetaType::type("QRegExp");
    } else if (typeId == QVariant::UserType) {
        QByteArray name;
        s >> name;
        typeId = QMetaType::type(name.constData());
        if (typeId == QMetaType::UnknownType) {
            s.setStatus(QDataStream::ReadCorruptData);
            qWarning("QVariant::load: unknown user type with name %s.", name.constData());
            return;
        }
    }
    create(typeId, nullptr);
    d.is_null = is_null;

    if (!isValid()) {
        if (s.version() < QDataStream::Qt_5_0) {
        // Since we wrote something, we should read something
            QString x;
            s >> x;
        }
        d.is_null = true;
        return;
    }

    // const cast is safe since we operate on a newly constructed variant
    void *data = const_cast<void *>(constData());
    if (!d.type().load(s, data)) {
        s.setStatus(QDataStream::ReadCorruptData);
        qWarning("QVariant::load: unable to load type %d.", d.typeId());
    }
}

/*!
    Internal function for saving a variant to the stream \a s. Use the
    stream operators instead.

    \internal
*/
void QVariant::save(QDataStream &s) const
{
    quint32 typeId = d.typeId();
    bool saveAsUserType = false;
    if (typeId >= QMetaType::User) {
        typeId = QMetaType::User;
        saveAsUserType = true;
    }
    if (s.version() < QDataStream::Qt_4_0) {
        int i;
        for (i = 0; i <= MapFromThreeCount - 1; ++i) {
            if (mapIdFromQt3ToCurrent[i] == typeId) {
                typeId = i;
                break;
            }
        }
        if (i >= MapFromThreeCount) {
            s << QVariant();
            return;
        }
    } else if (s.version() < QDataStream::Qt_5_0) {
        if (typeId == QMetaType::User) {
            typeId = 127; // QVariant::UserType had this value in Qt4
            saveAsUserType = true;
        } else if (typeId >= 128 - 97 && typeId <= LastCoreType) {
            // In Qt4 id == 128 was FirstExtCoreType. In Qt5 ExtCoreTypes set was merged to CoreTypes
            // by moving all ids down by 97.
            typeId += 97;
        } else if (typeId == QMetaType::QSizePolicy) {
            typeId = 75;
#if QT_CONFIG(shortcut)
        } else if (typeId >= QMetaType::QKeySequence && typeId <= QMetaType::QQuaternion) {
#else
        } else if (typeId >= QMetaType::QPen && typeId <= QMetaType::QQuaternion) {
#endif
            // and as a result these types received lower ids too
            typeId +=1;
        } else if (typeId == QMetaType::QPolygonF || typeId == QMetaType::QUuid) {
            // These existed in Qt 4 only as a custom type
            typeId = 127;
            saveAsUserType = true;
        }
    }
    const char *typeName = nullptr;
    if (saveAsUserType) {
        typeName = QMetaType::typeName(d.typeId());
        if (!strcmp(typeName, "QRegExp")) {
            typeId = 27; // QRegExp in Qt 4/5
            typeName = nullptr;
        }
    }
    s << typeId;
    if (s.version() >= QDataStream::Qt_4_2)
        s << qint8(d.is_null);
    if (typeName)
        s << QMetaType::typeName(userType());

    if (!isValid()) {
        if (s.version() < QDataStream::Qt_5_0)
            s << QString();
        return;
    }

    if (!d.type().save(s, constData())) {
        qWarning("QVariant::save: unable to save type '%s' (type id: %d).\n",
                 QMetaType::typeName(d.typeId()), d.typeId());
        Q_ASSERT_X(false, "QVariant::save", "Invalid type to save");
    }
}

/*!
    \since 4.4

    Reads a variant \a p from the stream \a s.

    \sa{Serializing Qt Data Types}{Format of the QDataStream operators}
*/
QDataStream& operator>>(QDataStream &s, QVariant &p)
{
    p.load(s);
    return s;
}

/*!
    Writes a variant \a p to the stream \a s.

    \sa{Serializing Qt Data Types}{Format of the QDataStream operators}
*/
QDataStream& operator<<(QDataStream &s, const QVariant &p)
{
    p.save(s);
    return s;
}

/*!
    Reads a variant type \a p in enum representation from the stream \a s.
*/
QDataStream& operator>>(QDataStream &s, QVariant::Type &p)
{
    quint32 u;
    s >> u;
    p = (QVariant::Type)u;

    return s;
}

/*!
    Writes a variant type \a p to the stream \a s.
*/
QDataStream& operator<<(QDataStream &s, const QVariant::Type p)
{
    s << static_cast<quint32>(p);

    return s;
}

#endif //QT_NO_DATASTREAM

/*!
    \fn bool QVariant::isValid() const

    Returns \c true if the storage type of this variant is not
    QMetaType::UnknownType; otherwise returns \c false.
*/

template<typename T>
inline T qVariantToHelper(const QVariant::Private &d)
{
    QMetaType targetType = QMetaType::fromType<T>();
    if (d.type() == targetType)
        return d.get<T>();

    T ret;
    const void * const from = d.storage();
    if (QMetaType::convert(from, d.typeId(), &ret, targetType.id()))
        return ret;

    convert(&d, targetType.id(), &ret);
    return ret;
}

/*!
    \fn QStringList QVariant::toStringList() const

    Returns the variant as a QStringList if the variant has userType()
    \l QMetaType::QStringList, \l QMetaType::QString, or
    \l QMetaType::QVariantList of a type that can be converted to QString;
    otherwise returns an empty list.

    \sa canConvert(int targetTypeId), convert()
*/
QStringList QVariant::toStringList() const
{
    return qVariantToHelper<QStringList>(d);
}

/*!
    Returns the variant as a QString if the variant has a userType()
    including, but not limited to:

    \l QMetaType::QString, \l QMetaType::Bool, \l QMetaType::QByteArray,
    \l QMetaType::QChar, \l QMetaType::QDate, \l QMetaType::QDateTime,
    \l QMetaType::Double, \l QMetaType::Int, \l QMetaType::LongLong,
    \l QMetaType::QStringList, \l QMetaType::QTime, \l QMetaType::UInt, or
    \l QMetaType::ULongLong.

    Calling QVariant::toString() on an unsupported variant returns an empty
    string.

    \sa canConvert(int targetTypeId), convert()
*/
QString QVariant::toString() const
{
    return qVariantToHelper<QString>(d);
}

/*!
    Returns the variant as a QMap<QString, QVariant> if the variant
    has type() \l QMetaType::QVariantMap; otherwise returns an empty map.

    \sa canConvert(int targetTypeId), convert()
*/
QVariantMap QVariant::toMap() const
{
    return qVariantToHelper<QVariantMap>(d);
}

/*!
    Returns the variant as a QHash<QString, QVariant> if the variant
    has type() \l QMetaType::QVariantHash; otherwise returns an empty map.

    \sa canConvert(int targetTypeId), convert()
*/
QVariantHash QVariant::toHash() const
{
    return qVariantToHelper<QVariantHash>(d);
}

/*!
    \fn QDate QVariant::toDate() const

    Returns the variant as a QDate if the variant has userType()
    \l QMetaType::QDate, \l QMetaType::QDateTime, or \l QMetaType::QString;
    otherwise returns an invalid date.

    If the type() is \l QMetaType::QString, an invalid date will be returned if
    the string cannot be parsed as a Qt::ISODate format date.

    \sa canConvert(int targetTypeId), convert()
*/
QDate QVariant::toDate() const
{
    return qVariantToHelper<QDate>(d);
}

/*!
    \fn QTime QVariant::toTime() const

    Returns the variant as a QTime if the variant has userType()
    \l QMetaType::QTime, \l QMetaType::QDateTime, or \l QMetaType::QString;
    otherwise returns an invalid time.

    If the type() is \l QMetaType::QString, an invalid time will be returned if
    the string cannot be parsed as a Qt::ISODate format time.

    \sa canConvert(int targetTypeId), convert()
*/
QTime QVariant::toTime() const
{
    return qVariantToHelper<QTime>(d);
}

/*!
    \fn QDateTime QVariant::toDateTime() const

    Returns the variant as a QDateTime if the variant has userType()
    \l QMetaType::QDateTime, \l QMetaType::QDate, or \l QMetaType::QString;
    otherwise returns an invalid date/time.

    If the type() is \l QMetaType::QString, an invalid date/time will be
    returned if the string cannot be parsed as a Qt::ISODate format date/time.

    \sa canConvert(int targetTypeId), convert()
*/
QDateTime QVariant::toDateTime() const
{
    return qVariantToHelper<QDateTime>(d);
}

/*!
    \since 4.7
    \fn QEasingCurve QVariant::toEasingCurve() const

    Returns the variant as a QEasingCurve if the variant has userType()
    \l QMetaType::QEasingCurve; otherwise returns a default easing curve.

    \sa canConvert(int targetTypeId), convert()
*/
#if QT_CONFIG(easingcurve)
QEasingCurve QVariant::toEasingCurve() const
{
    return qVariantToHelper<QEasingCurve>(d);
}
#endif

/*!
    \fn QByteArray QVariant::toByteArray() const

    Returns the variant as a QByteArray if the variant has userType()
    \l QMetaType::QByteArray or \l QMetaType::QString (converted using
    QString::fromUtf8()); otherwise returns an empty byte array.

    \sa canConvert(int targetTypeId), convert()
*/
QByteArray QVariant::toByteArray() const
{
    return qVariantToHelper<QByteArray>(d);
}

#ifndef QT_NO_GEOM_VARIANT
/*!
    \fn QPoint QVariant::toPoint() const

    Returns the variant as a QPoint if the variant has userType()
    \l QMetaType::QPoint or \l QMetaType::QPointF; otherwise returns a null
    QPoint.

    \sa canConvert(int targetTypeId), convert()
*/
QPoint QVariant::toPoint() const
{
    return qVariantToHelper<QPoint>(d);
}

/*!
    \fn QRect QVariant::toRect() const

    Returns the variant as a QRect if the variant has userType()
    \l QMetaType::QRect; otherwise returns an invalid QRect.

    \sa canConvert(int targetTypeId), convert()
*/
QRect QVariant::toRect() const
{
    return qVariantToHelper<QRect>(d);
}

/*!
    \fn QSize QVariant::toSize() const

    Returns the variant as a QSize if the variant has userType()
    \l QMetaType::QSize; otherwise returns an invalid QSize.

    \sa canConvert(int targetTypeId), convert()
*/
QSize QVariant::toSize() const
{
    return qVariantToHelper<QSize>(d);
}

/*!
    \fn QSizeF QVariant::toSizeF() const

    Returns the variant as a QSizeF if the variant has userType() \l
    QMetaType::QSizeF; otherwise returns an invalid QSizeF.

    \sa canConvert(int targetTypeId), convert()
*/
QSizeF QVariant::toSizeF() const
{
    return qVariantToHelper<QSizeF>(d);
}

/*!
    \fn QRectF QVariant::toRectF() const

    Returns the variant as a QRectF if the variant has userType()
    \l QMetaType::QRect or \l QMetaType::QRectF; otherwise returns an invalid
    QRectF.

    \sa canConvert(int targetTypeId), convert()
*/
QRectF QVariant::toRectF() const
{
    return qVariantToHelper<QRectF>(d);
}

/*!
    \fn QLineF QVariant::toLineF() const

    Returns the variant as a QLineF if the variant has userType()
    \l QMetaType::QLineF; otherwise returns an invalid QLineF.

    \sa canConvert(int targetTypeId), convert()
*/
QLineF QVariant::toLineF() const
{
    return qVariantToHelper<QLineF>(d);
}

/*!
    \fn QLine QVariant::toLine() const

    Returns the variant as a QLine if the variant has userType()
    \l QMetaType::QLine; otherwise returns an invalid QLine.

    \sa canConvert(int targetTypeId), convert()
*/
QLine QVariant::toLine() const
{
    return qVariantToHelper<QLine>(d);
}

/*!
    \fn QPointF QVariant::toPointF() const

    Returns the variant as a QPointF if the variant has userType() \l
    QMetaType::QPoint or \l QMetaType::QPointF; otherwise returns a null
    QPointF.

    \sa canConvert(int targetTypeId), convert()
*/
QPointF QVariant::toPointF() const
{
    return qVariantToHelper<QPointF>(d);
}

#endif // QT_NO_GEOM_VARIANT

#ifndef QT_BOOTSTRAPPED
/*!
    \fn QUrl QVariant::toUrl() const

    Returns the variant as a QUrl if the variant has userType()
    \l QMetaType::QUrl; otherwise returns an invalid QUrl.

    \sa canConvert(int targetTypeId), convert()
*/
QUrl QVariant::toUrl() const
{
    return qVariantToHelper<QUrl>(d);
}
#endif

/*!
    \fn QLocale QVariant::toLocale() const

    Returns the variant as a QLocale if the variant has userType()
    \l QMetaType::QLocale; otherwise returns an invalid QLocale.

    \sa canConvert(int targetTypeId), convert()
*/
QLocale QVariant::toLocale() const
{
    return qVariantToHelper<QLocale>(d);
}

#if QT_CONFIG(regularexpression)
/*!
    \fn QRegularExpression QVariant::toRegularExpression() const
    \since 5.0

    Returns the variant as a QRegularExpression if the variant has userType() \l
    QRegularExpression; otherwise returns an empty QRegularExpression.

    \sa canConvert(int targetTypeId), convert()
*/
QRegularExpression QVariant::toRegularExpression() const
{
    return qVariantToHelper<QRegularExpression>(d);
}
#endif // QT_CONFIG(regularexpression)

#if QT_CONFIG(itemmodel)
/*!
    \since 5.0

    Returns the variant as a QModelIndex if the variant has userType() \l
    QModelIndex; otherwise returns a default constructed QModelIndex.

    \sa canConvert(int targetTypeId), convert(), toPersistentModelIndex()
*/
QModelIndex QVariant::toModelIndex() const
{
    return qVariantToHelper<QModelIndex>(d);
}

/*!
    \since 5.5

    Returns the variant as a QPersistentModelIndex if the variant has userType() \l
    QPersistentModelIndex; otherwise returns a default constructed QPersistentModelIndex.

    \sa canConvert(int targetTypeId), convert(), toModelIndex()
*/
QPersistentModelIndex QVariant::toPersistentModelIndex() const
{
    return qVariantToHelper<QPersistentModelIndex>(d);
}
#endif // QT_CONFIG(itemmodel)

/*!
    \since 5.0

    Returns the variant as a QUuid if the variant has type()
    \l QMetaType::QUuid, \l QMetaType::QByteArray or \l QMetaType::QString;
    otherwise returns a default-constructed QUuid.

    \sa canConvert(int targetTypeId), convert()
*/
QUuid QVariant::toUuid() const
{
    return qVariantToHelper<QUuid>(d);
}

#ifndef QT_BOOTSTRAPPED
/*!
    \since 5.0

    Returns the variant as a QJsonValue if the variant has userType() \l
    QJsonValue; otherwise returns a default constructed QJsonValue.

    \sa canConvert(int targetTypeId), convert()
*/
QJsonValue QVariant::toJsonValue() const
{
    return qVariantToHelper<QJsonValue>(d);
}

/*!
    \since 5.0

    Returns the variant as a QJsonObject if the variant has userType() \l
    QJsonObject; otherwise returns a default constructed QJsonObject.

    \sa canConvert(int targetTypeId), convert()
*/
QJsonObject QVariant::toJsonObject() const
{
    return qVariantToHelper<QJsonObject>(d);
}

/*!
    \since 5.0

    Returns the variant as a QJsonArray if the variant has userType() \l
    QJsonArray; otherwise returns a default constructed QJsonArray.

    \sa canConvert(int targetTypeId), convert()
*/
QJsonArray QVariant::toJsonArray() const
{
    return qVariantToHelper<QJsonArray>(d);
}

/*!
    \since 5.0

    Returns the variant as a QJsonDocument if the variant has userType() \l
    QJsonDocument; otherwise returns a default constructed QJsonDocument.

    \sa canConvert(int targetTypeId), convert()
*/
QJsonDocument QVariant::toJsonDocument() const
{
    return qVariantToHelper<QJsonDocument>(d);
}
#endif // QT_BOOTSTRAPPED

/*!
    \fn QChar QVariant::toChar() const

    Returns the variant as a QChar if the variant has userType()
    \l QMetaType::QChar, \l QMetaType::Int, or \l QMetaType::UInt; otherwise
    returns an invalid QChar.

    \sa canConvert(int targetTypeId), convert()
*/
QChar QVariant::toChar() const
{
    return qVariantToHelper<QChar>(d);
}

/*!
    Returns the variant as a QBitArray if the variant has userType()
    \l QMetaType::QBitArray; otherwise returns an empty bit array.

    \sa canConvert(int targetTypeId), convert()
*/
QBitArray QVariant::toBitArray() const
{
    return qVariantToHelper<QBitArray>(d);
}

template <typename T>
inline T qNumVariantToHelper(const QVariant::Private &d, bool *ok, const T& val)
{
    const uint t = qMetaTypeId<T>();
    if (ok)
        *ok = true;

    if (d.typeId() == t)
        return val;

    T ret = 0;
    if (QMetaType::convert(d.storage(), d.typeId(), &ret, t))
        return ret;

    bool success = convert(&d, t, &ret);
    if (ok)
        *ok = success;
    return ret;
}

/*!
    Returns the variant as an int if the variant has userType()
    \l QMetaType::Int, \l QMetaType::Bool, \l QMetaType::QByteArray,
    \l QMetaType::QChar, \l QMetaType::Double, \l QMetaType::LongLong,
    \l QMetaType::QString, \l QMetaType::UInt, or \l QMetaType::ULongLong;
    otherwise returns 0.

    If \a ok is non-null: \c{*}\a{ok} is set to true if the value could be
    converted to an int; otherwise \c{*}\a{ok} is set to false.

    \b{Warning:} If the value is convertible to a \l QMetaType::LongLong but is
    too large to be represented in an int, the resulting arithmetic overflow
    will not be reflected in \a ok. A simple workaround is to use
    QString::toInt().

    \sa canConvert(int targetTypeId), convert()
*/
int QVariant::toInt(bool *ok) const
{
    return qNumVariantToHelper<int>(d, ok, d.get<int>());
}

/*!
    Returns the variant as an unsigned int if the variant has userType()
    \l QMetaType::UInt, \l QMetaType::Bool, \l QMetaType::QByteArray,
    \l QMetaType::QChar, \l QMetaType::Double, \l QMetaType::Int,
    \l QMetaType::LongLong, \l QMetaType::QString, or \l QMetaType::ULongLong;
    otherwise returns 0.

    If \a ok is non-null: \c{*}\a{ok} is set to true if the value could be
    converted to an unsigned int; otherwise \c{*}\a{ok} is set to false.

    \b{Warning:} If the value is convertible to a \l QMetaType::ULongLong but is
    too large to be represented in an unsigned int, the resulting arithmetic
    overflow will not be reflected in \a ok. A simple workaround is to use
    QString::toUInt().

    \sa canConvert(int targetTypeId), convert()
*/
uint QVariant::toUInt(bool *ok) const
{
    return qNumVariantToHelper<uint>(d, ok, d.get<unsigned int>());
}

/*!
    Returns the variant as a long long int if the variant has userType()
    \l QMetaType::LongLong, \l QMetaType::Bool, \l QMetaType::QByteArray,
    \l QMetaType::QChar, \l QMetaType::Double, \l QMetaType::Int,
    \l QMetaType::QString, \l QMetaType::UInt, or \l QMetaType::ULongLong;
    otherwise returns 0.

    If \a ok is non-null: \c{*}\c{ok} is set to true if the value could be
    converted to an int; otherwise \c{*}\c{ok} is set to false.

    \sa canConvert(int targetTypeId), convert()
*/
qlonglong QVariant::toLongLong(bool *ok) const
{
    return qNumVariantToHelper<qlonglong>(d, ok, d.get<qlonglong>());
}

/*!
    Returns the variant as an unsigned long long int if the
    variant has type() \l QMetaType::ULongLong, \l QMetaType::Bool,
    \l QMetaType::QByteArray, \l QMetaType::QChar, \l QMetaType::Double,
    \l QMetaType::Int, \l QMetaType::LongLong, \l QMetaType::QString, or
    \l QMetaType::UInt; otherwise returns 0.

    If \a ok is non-null: \c{*}\a{ok} is set to true if the value could be
    converted to an int; otherwise \c{*}\a{ok} is set to false.

    \sa canConvert(int targetTypeId), convert()
*/
qulonglong QVariant::toULongLong(bool *ok) const
{
    return qNumVariantToHelper<qulonglong>(d, ok, d.get<qulonglong>());
}

/*!
    Returns the variant as a bool if the variant has userType() Bool.

    Returns \c true if the variant has userType() \l QMetaType::Bool,
    \l QMetaType::QChar, \l QMetaType::Double, \l QMetaType::Int,
    \l QMetaType::LongLong, \l QMetaType::UInt, or \l QMetaType::ULongLong and
    the value is non-zero, or if the variant has type \l QMetaType::QString or
    \l QMetaType::QByteArray and its lower-case content is not one of the
    following: empty, "0" or "false"; otherwise returns \c false.

    \sa canConvert(int targetTypeId), convert()
*/
bool QVariant::toBool() const
{
    if (d.type() == QMetaType::fromType<bool>())
        return d.get<bool>();

    bool res = false;

    if (QMetaType::convert(constData(), d.typeId(), &res, QMetaType::Bool))
        return res;

    ::convert(&d, Bool, &res);

    return res;
}

/*!
    Returns the variant as a double if the variant has userType()
    \l QMetaType::Double, \l QMetaType::Float, \l QMetaType::Bool,
    \l QMetaType::QByteArray, \l QMetaType::Int, \l QMetaType::LongLong,
    \l QMetaType::QString, \l QMetaType::UInt, or \l QMetaType::ULongLong;
    otherwise returns 0.0.

    If \a ok is non-null: \c{*}\a{ok} is set to true if the value could be
    converted to a double; otherwise \c{*}\a{ok} is set to false.

    \sa canConvert(int targetTypeId), convert()
*/
double QVariant::toDouble(bool *ok) const
{
    return qNumVariantToHelper<double>(d, ok, d.get<double>());
}

/*!
    Returns the variant as a float if the variant has userType()
    \l QMetaType::Double, \l QMetaType::Float, \l QMetaType::Bool,
    \l QMetaType::QByteArray, \l QMetaType::Int, \l QMetaType::LongLong,
    \l QMetaType::QString, \l QMetaType::UInt, or \l QMetaType::ULongLong;
    otherwise returns 0.0.

    \since 4.6

    If \a ok is non-null: \c{*}\a{ok} is set to true if the value could be
    converted to a double; otherwise \c{*}\a{ok} is set to false.

    \sa canConvert(int targetTypeId), convert()
*/
float QVariant::toFloat(bool *ok) const
{
    return qNumVariantToHelper<float>(d, ok, d.get<float>());
}

/*!
    Returns the variant as a qreal if the variant has userType()
    \l QMetaType::Double, \l QMetaType::Float, \l QMetaType::Bool,
    \l QMetaType::QByteArray, \l QMetaType::Int, \l QMetaType::LongLong,
    \l QMetaType::QString, \l QMetaType::UInt, or \l QMetaType::ULongLong;
    otherwise returns 0.0.

    \since 4.6

    If \a ok is non-null: \c{*}\a{ok} is set to true if the value could be
    converted to a double; otherwise \c{*}\a{ok} is set to false.

    \sa canConvert(int targetTypeId), convert()
*/
qreal QVariant::toReal(bool *ok) const
{
    return qNumVariantToHelper<qreal>(d, ok, d.get<qreal>());
}

/*!
    Returns the variant as a QVariantList if the variant has userType()
    \l QMetaType::QVariantList or \l QMetaType::QStringList; otherwise returns
    an empty list.

    \sa canConvert(int targetTypeId), convert()
*/
QVariantList QVariant::toList() const
{
    return qVariantToHelper<QVariantList>(d);
}


static const quint32 qCanConvertMatrix[QMetaType::LastCoreType + 1] =
{
/*Invalid*/     0,

/*Bool*/          1 << QMetaType::Double     | 1 << QMetaType::Int        | 1 << QMetaType::UInt
                | 1 << QMetaType::LongLong   | 1 << QMetaType::ULongLong  | 1 << QMetaType::QByteArray
                | 1 << QMetaType::QString     | 1 << QMetaType::QChar,

/*Int*/           1 << QMetaType::UInt       | 1 << QMetaType::QString     | 1 << QMetaType::Double
                | 1 << QMetaType::Bool       | 1 << QMetaType::LongLong   | 1 << QMetaType::ULongLong
                | 1 << QMetaType::QChar       | 1 << QMetaType::QByteArray  | 1 << QMetaType::Int,

/*UInt*/          1 << QMetaType::Int        | 1 << QMetaType::QString     | 1 << QMetaType::Double
                | 1 << QMetaType::Bool       | 1 << QMetaType::LongLong   | 1 << QMetaType::ULongLong
                | 1 << QMetaType::QChar       | 1 << QMetaType::QByteArray,

/*LLong*/         1 << QMetaType::Int        | 1 << QMetaType::QString     | 1 << QMetaType::Double
                | 1 << QMetaType::Bool       | 1 << QMetaType::UInt       | 1 << QMetaType::ULongLong
                | 1 << QMetaType::QChar       | 1 << QMetaType::QByteArray,

/*ULlong*/        1 << QMetaType::Int        | 1 << QMetaType::QString     | 1 << QMetaType::Double
                | 1 << QMetaType::Bool       | 1 << QMetaType::UInt       | 1 << QMetaType::LongLong
                | 1 << QMetaType::QChar       | 1 << QMetaType::QByteArray,

/*double*/        1 << QMetaType::Int        | 1 << QMetaType::QString     | 1 << QMetaType::ULongLong
                | 1 << QMetaType::Bool       | 1 << QMetaType::UInt       | 1 << QMetaType::LongLong
                | 1 << QMetaType::QByteArray,

/*QChar*/         1 << QMetaType::Int        | 1 << QMetaType::UInt       | 1 << QMetaType::LongLong
                | 1 << QMetaType::ULongLong,

/*QMap*/          0,

/*QList*/         1 << QMetaType::QStringList,

/*QString*/       1 << QMetaType::QStringList | 1 << QMetaType::QByteArray  | 1 << QMetaType::Int
                | 1 << QMetaType::UInt       | 1 << QMetaType::Bool       | 1 << QMetaType::Double
                | 1 << QMetaType::QDate       | 1 << QMetaType::QTime       | 1 << QMetaType::QDateTime
                | 1 << QMetaType::LongLong   | 1 << QMetaType::ULongLong  | 1 << QMetaType::QChar
                | 1 << QMetaType::QUrl        | 1 << QMetaType::QUuid,

/*QStringList*/   1 << QMetaType::QVariantList       | 1 << QMetaType::QString,

/*QByteArray*/    1 << QMetaType::QString     | 1 << QMetaType::Int        | 1 << QMetaType::UInt | 1 << QMetaType::Bool
                | 1 << QMetaType::Double     | 1 << QMetaType::LongLong   | 1 << QMetaType::ULongLong
                | 1 << QMetaType::QUuid,

/*QBitArray*/     0,

/*QDate*/         1 << QMetaType::QString     | 1 << QMetaType::QDateTime,

/*QTime*/         1 << QMetaType::QString     | 1 << QMetaType::QDateTime,

/*QDateTime*/     1 << QMetaType::QString     | 1 << QMetaType::QDate,

/*QUrl*/          1 << QMetaType::QString,

/*QLocale*/       0,

/*QRect*/         1 << QMetaType::QRectF,

/*QRectF*/        1 << QMetaType::QRect,

/*QSize*/         1 << QMetaType::QSizeF,

/*QSizeF*/        1 << QMetaType::QSize,

/*QLine*/         1 << QMetaType::QLineF,

/*QLineF*/        1 << QMetaType::QLine,

/*QPoint*/        1 << QMetaType::QPointF,

/*QPointF*/       1 << QMetaType::QPoint,

/*unused, was: QRegExp*/       0,

/*QHash*/         0,

/*QEasingCurve*/  0,

/*QUuid*/         1 << QMetaType::QString     | 1 << QMetaType::QByteArray,
};
static const size_t qCanConvertMatrixMaximumTargetType = 8 * sizeof(*qCanConvertMatrix);

#ifndef QT_BOOTSTRAPPED
/*
    Returns \c true if from inherits to.
*/
static bool canConvertMetaObject(const QMetaObject *from, const QMetaObject *to)
{
    if (from && to == &QObject::staticMetaObject)
        return true;

    while (from) {
        if (from == to)
            return true;
        from = from->superClass();
    }

    return false;
}
#endif

static bool canConvertMetaObject(int fromId, int toId, QObject *fromObject)
{
#ifndef QT_BOOTSTRAPPED
    QMetaType toType(toId);
    if ((QMetaType::typeFlags(fromId) & QMetaType::PointerToQObject) && (toType.flags() & QMetaType::PointerToQObject)) {
        if (!fromObject)
            return true;
        return canConvertMetaObject(fromObject->metaObject(), toType.metaObject());
    }
#else
    Q_UNUSED(fromId);
    Q_UNUSED(toId);
    Q_UNUSED(fromObject);
#endif
    return false;
}


/*!
    Returns \c true if the variant's type can be cast to the requested
    type, \a targetTypeId. Such casting is done automatically when calling the
    toInt(), toBool(), ... methods.

    The following casts are done automatically:

    \table
    \header \li Type \li Automatically Cast To
    \row \li \l QMetaType::Bool \li \l QMetaType::QChar, \l QMetaType::Double,
        \l QMetaType::Int, \l QMetaType::LongLong, \l QMetaType::QString,
        \l QMetaType::UInt, \l QMetaType::ULongLong
    \row \li \l QMetaType::QByteArray \li \l QMetaType::Double,
        \l QMetaType::Int, \l QMetaType::LongLong, \l QMetaType::QString,
        \l QMetaType::UInt, \l QMetaType::ULongLong, \l QMetaType::QUuid
    \row \li \l QMetaType::QChar \li \l QMetaType::Bool, \l QMetaType::Int,
        \l QMetaType::UInt, \l QMetaType::LongLong, \l QMetaType::ULongLong
    \row \li \l QMetaType::QColor \li \l QMetaType::QString
    \row \li \l QMetaType::QDate \li \l QMetaType::QDateTime,
        \l QMetaType::QString
    \row \li \l QMetaType::QDateTime \li \l QMetaType::QDate,
        \l QMetaType::QString, \l QMetaType::QTime
    \row \li \l QMetaType::Double \li \l QMetaType::Bool, \l QMetaType::Int,
        \l QMetaType::LongLong, \l QMetaType::QString, \l QMetaType::UInt,
        \l QMetaType::ULongLong
    \row \li \l QMetaType::QFont \li \l QMetaType::QString
    \row \li \l QMetaType::Int \li \l QMetaType::Bool, \l QMetaType::QChar,
        \l QMetaType::Double, \l QMetaType::LongLong, \l QMetaType::QString,
        \l QMetaType::UInt, \l QMetaType::ULongLong
    \row \li \l QMetaType::QKeySequence \li \l QMetaType::Int,
        \l QMetaType::QString
    \row \li \l QMetaType::QVariantList \li \l QMetaType::QStringList (if the
        list's items can be converted to QStrings)
    \row \li \l QMetaType::LongLong \li \l QMetaType::Bool,
        \l QMetaType::QByteArray, \l QMetaType::QChar, \l QMetaType::Double,
        \l QMetaType::Int, \l QMetaType::QString, \l QMetaType::UInt,
        \l QMetaType::ULongLong
    \row \li \l QMetaType::QPoint \li QMetaType::QPointF
    \row \li \l QMetaType::QRect \li QMetaType::QRectF
    \row \li \l QMetaType::QString \li \l QMetaType::Bool,
        \l QMetaType::QByteArray, \l QMetaType::QChar, \l QMetaType::QColor,
        \l QMetaType::QDate, \l QMetaType::QDateTime, \l QMetaType::Double,
        \l QMetaType::QFont, \l QMetaType::Int, \l QMetaType::QKeySequence,
        \l QMetaType::LongLong, \l QMetaType::QStringList, \l QMetaType::QTime,
        \l QMetaType::UInt, \l QMetaType::ULongLong, \l QMetaType::QUuid
    \row \li \l QMetaType::QStringList \li \l QMetaType::QVariantList,
        \l QMetaType::QString (if the list contains exactly one item)
    \row \li \l QMetaType::QTime \li \l QMetaType::QString
    \row \li \l QMetaType::UInt \li \l QMetaType::Bool, \l QMetaType::QChar,
        \l QMetaType::Double, \l QMetaType::Int, \l QMetaType::LongLong,
        \l QMetaType::QString, \l QMetaType::ULongLong
    \row \li \l QMetaType::ULongLong \li \l QMetaType::Bool,
        \l QMetaType::QChar, \l QMetaType::Double, \l QMetaType::Int,
        \l QMetaType::LongLong, \l QMetaType::QString, \l QMetaType::UInt
    \row \li \l QMetaType::QUuid \li \l QMetaType::QByteArray, \l QMetaType::QString
    \endtable

    A QVariant containing a pointer to a type derived from QObject will also return true for this
    function if a qobject_cast to the type described by \a targetTypeId would succeed. Note that
    this only works for QObject subclasses which use the Q_OBJECT macro.

    A QVariant containing a sequential container will also return true for this
    function if the \a targetTypeId is QVariantList. It is possible to iterate over
    the contents of the container without extracting it as a (copied) QVariantList:

    \snippet code/src_corelib_kernel_qvariant.cpp 9

    This requires that the value_type of the container is itself a metatype.

    Similarly, a QVariant containing a sequential container will also return true for this
    function the \a targetTypeId is QVariantHash or QVariantMap. It is possible to iterate over
    the contents of the container without extracting it as a (copied) QVariantHash or QVariantMap:

    \snippet code/src_corelib_kernel_qvariant.cpp 10

    \sa convert(), QSequentialIterable, Q_DECLARE_SEQUENTIAL_CONTAINER_METATYPE(), QAssociativeIterable,
        Q_DECLARE_ASSOCIATIVE_CONTAINER_METATYPE()
*/
bool QVariant::canConvert(int targetTypeId) const
{
    if (d.typeId() == targetTypeId)
        return true;

#if QT_CONFIG(itemmodel)
    if ((targetTypeId == QMetaType::QModelIndex
         && d.typeId() == QMetaType::QPersistentModelIndex)
        || (targetTypeId == QMetaType::QPersistentModelIndex
            && d.typeId() == QMetaType::QModelIndex))
        return true;
#endif

    if (targetTypeId == QMetaType::QVariantList
        && (d.typeId() == QMetaType::QVariantList || d.typeId() == QMetaType::QStringList
            || d.typeId() == QMetaType::QByteArrayList
            || QMetaType::hasRegisteredConverterFunction(
                    d.typeId(), qMetaTypeId<QtMetaTypePrivate::QSequentialIterableImpl>()))) {
        return true;
    }

    if ((targetTypeId == QMetaType::QVariantHash || targetTypeId == QMetaType::QVariantMap)
        && (d.typeId() == QMetaType::QVariantMap || d.typeId() == QMetaType::QVariantHash
            || QMetaType::hasRegisteredConverterFunction(
                    d.typeId(), qMetaTypeId<QtMetaTypePrivate::QAssociativeIterableImpl>()))) {
        return true;
    }

    if (targetTypeId == qMetaTypeId<QPair<QVariant, QVariant>>()
        && QMetaType::hasRegisteredConverterFunction(
                d.typeId(), qMetaTypeId<QtMetaTypePrivate::QPairVariantInterfaceImpl>())) {
        return true;
    }

    if ((d.typeId() >= QMetaType::LastCoreType|| targetTypeId >= QMetaType::LastCoreType)
        && QMetaType::hasRegisteredConverterFunction(d.typeId(), targetTypeId)) {
        return true;
    }

    // TODO Reimplement this function, currently it works but it is a historical mess.
    uint currentType = d.typeId();
    if (currentType == QMetaType::SChar || currentType == QMetaType::Char)
        currentType = QMetaType::UInt;
    if (targetTypeId == QMetaType::SChar || currentType == QMetaType::Char)
        targetTypeId = QMetaType::UInt;
    if (currentType == QMetaType::Short || currentType == QMetaType::UShort)
        currentType = QMetaType::Int;
    if (targetTypeId == QMetaType::Short || currentType == QMetaType::UShort)
        targetTypeId = QMetaType::Int;
    if (currentType == QMetaType::Float)
        currentType = QMetaType::Double;
    if (targetTypeId == QMetaType::Float)
        targetTypeId = QMetaType::Double;

    if (currentType == uint(targetTypeId))
        return true;

    if (targetTypeId < 0)
        return false;
    if (targetTypeId >= QMetaType::User) {
        if (QMetaType::typeFlags(targetTypeId) & QMetaType::IsEnumeration) {
            targetTypeId = QMetaType::Int;
        } else {
            return canConvertMetaObject(currentType, targetTypeId, d.get<QObject *>());
        }
    }

    if (currentType == QMetaType::QJsonValue || targetTypeId == QMetaType::QJsonValue) {
        switch (currentType == QMetaType::QJsonValue ? targetTypeId : currentType) {
        case QMetaType::Nullptr:
        case QMetaType::QString:
        case QMetaType::Bool:
        case QMetaType::Int:
        case QMetaType::UInt:
        case QMetaType::Double:
        case QMetaType::Float:
        case QMetaType::ULong:
        case QMetaType::Long:
        case QMetaType::LongLong:
        case QMetaType::ULongLong:
        case QMetaType::UShort:
        case QMetaType::UChar:
        case QMetaType::Char:
        case QMetaType::SChar:
        case QMetaType::Short:
        case QMetaType::QVariantList:
        case QMetaType::QVariantMap:
        case QMetaType::QVariantHash:
        case QMetaType::QCborValue:
        case QMetaType::QCborArray:
        case QMetaType::QCborMap:
            return true;
        default:
            return false;
        }
    }
    if (currentType == QMetaType::QJsonArray)
        return targetTypeId == QMetaType::QVariantList || targetTypeId == QMetaType::QCborValue
                || targetTypeId == QMetaType::QCborArray;
    if (currentType == QMetaType::QJsonObject)
        return targetTypeId == QMetaType::QVariantMap || targetTypeId == QMetaType::QVariantHash
                || targetTypeId == QMetaType::QCborValue || targetTypeId == QMetaType::QCborMap;

    if (currentType == QMetaType::QCborValue || targetTypeId == QMetaType::QCborValue) {
        switch (currentType == QMetaType::QCborValue ? targetTypeId : currentType) {
        case QMetaType::UnknownType:
        case QMetaType::Nullptr:
        case QMetaType::Bool:
        case QMetaType::Int:
        case QMetaType::UInt:
        case QMetaType::Double:
        case QMetaType::Float:
        case QMetaType::ULong:
        case QMetaType::Long:
        case QMetaType::LongLong:
        case QMetaType::ULongLong:
        case QMetaType::UShort:
        case QMetaType::UChar:
        case QMetaType::Char:
        case QMetaType::SChar:
        case QMetaType::Short:
        case QMetaType::QString:
        case QMetaType::QByteArray:
        case QMetaType::QDateTime:
        case QMetaType::QUrl:
#if QT_CONFIG(regularexpression)
        case QMetaType::QRegularExpression:
#endif
        case QMetaType::QUuid:
        case QMetaType::QVariantList:
        case QMetaType::QVariantMap:
        case QMetaType::QVariantHash:
        case QMetaType::QJsonValue:
        case QMetaType::QJsonArray:
        case QMetaType::QJsonObject:
        case QMetaType::QJsonDocument:
        case QMetaType::QCborArray:
        case QMetaType::QCborMap:
        case QMetaType::QCborSimpleType:
            return true;
        default:
            return false;
        }
    }
    if (currentType == QMetaType::QCborArray)
        return targetTypeId == QMetaType::QVariantList || targetTypeId == QMetaType::QCborValue
                || targetTypeId == QMetaType::QJsonArray;
    if (currentType == QMetaType::QCborMap)
        return targetTypeId == QMetaType::QVariantMap || targetTypeId == QMetaType::QVariantHash
                || targetTypeId == QMetaType::QCborValue || targetTypeId == QMetaType::QJsonObject;

    // FIXME It should be LastCoreType intead of Uuid
    if (currentType > int(QMetaType::QUuid) || targetTypeId > int(QMetaType::QUuid)) {
        switch (uint(targetTypeId)) {
        case QVariant::Int:
#if QT_CONFIG(shortcut)
            if (currentType == QVariant::KeySequence)
                return true;
            Q_FALLTHROUGH();
#endif
        case QVariant::UInt:
        case QVariant::LongLong:
        case QVariant::ULongLong:
               return currentType == QMetaType::ULong
                   || currentType == QMetaType::Long
                   || currentType == QMetaType::UShort
                   || currentType == QMetaType::UChar
                   || currentType == QMetaType::Char
                   || currentType == QMetaType::SChar
                   || currentType == QMetaType::Short
                   || QMetaType::typeFlags(currentType) & QMetaType::IsEnumeration;
        case QVariant::Image:
            return currentType == QVariant::Pixmap || currentType == QVariant::Bitmap;
        case QVariant::Pixmap:
            return currentType == QVariant::Image || currentType == QVariant::Bitmap
                              || currentType == QVariant::Brush;
        case QVariant::Bitmap:
            return currentType == QVariant::Pixmap || currentType == QVariant::Image;
        case QVariant::ByteArray:
            return currentType == QVariant::Color || currentType == QMetaType::Nullptr
                              || ((QMetaType::typeFlags(currentType) & QMetaType::IsEnumeration) && QMetaType::metaObjectForType(currentType));
        case QVariant::String:
            return currentType == QVariant::Font
                   || currentType == QVariant::Color || currentType == QMetaType::Nullptr
#if QT_CONFIG(shortcut)
                   || currentType == QVariant::KeySequence
#endif
                   || ((QMetaType::typeFlags(currentType) & QMetaType::IsEnumeration) && QMetaType::metaObjectForType(currentType));
#if QT_CONFIG(shortcut)
        case QVariant::KeySequence:
            return currentType == QVariant::String || currentType == QVariant::Int;
#endif
        case QVariant::Font:
            return currentType == QVariant::String;
        case QVariant::Color:
            return currentType == QVariant::String || currentType == QVariant::ByteArray
                              || currentType == QVariant::Brush;
        case QVariant::Brush:
            return currentType == QVariant::Color || currentType == QVariant::Pixmap;
        case QMetaType::Long:
        case QMetaType::Char:
        case QMetaType::SChar:
        case QMetaType::UChar:
        case QMetaType::ULong:
        case QMetaType::Short:
        case QMetaType::UShort:
            return currentType == QVariant::Int
                || (currentType < qCanConvertMatrixMaximumTargetType
                    && qCanConvertMatrix[QVariant::Int] & (1U << currentType))
                || QMetaType::typeFlags(currentType) & QMetaType::IsEnumeration;
        case QMetaType::QObjectStar:
            return canConvertMetaObject(currentType, targetTypeId, d.get<QObject *>());
        default:
            return false;
        }
    }

    if (targetTypeId == String && currentType == StringList)
        return d.get<QStringList>().count() == 1;
    return currentType < qCanConvertMatrixMaximumTargetType
        && qCanConvertMatrix[targetTypeId] & (1U << currentType);
}

/*!
    Casts the variant to the requested type, \a targetTypeId. If the cast cannot be
    done, the variant is still changed to the requested type, but is left in a cleared
    null state similar to that constructed by QVariant(Type).

    Returns \c true if the current type of the variant was successfully cast;
    otherwise returns \c false.

    A QVariant containing a pointer to a type derived from QObject will also convert
    and return true for this function if a qobject_cast to the type described
    by \a targetTypeId would succeed. Note that this only works for QObject subclasses
    which use the Q_OBJECT macro.

    \note converting QVariants that are null due to not being initialized or having
    failed a previous conversion will always fail, changing the type, remaining null,
    and returning \c false.

    \sa canConvert(int targetTypeId), clear()
*/

bool QVariant::convert(int targetTypeId)
{
    if (d.typeId() == targetTypeId)
        return true;

    QVariant oldValue = *this;

    clear();
    if (!oldValue.canConvert(targetTypeId))
        return false;

    create(targetTypeId, nullptr);
    // Fail if the value is not initialized or was forced null by a previous failed convert.
    if (oldValue.d.is_null && oldValue.d.typeId() != QMetaType::Nullptr)
        return false;

    if ((QMetaType::typeFlags(oldValue.userType()) & QMetaType::PointerToQObject) && (QMetaType::typeFlags(targetTypeId) & QMetaType::PointerToQObject)) {
        create(targetTypeId, &oldValue.d.get<QObject *>());
        return true;
    }

    bool isOk = false;
    isOk = QMetaType::convert(oldValue.constData(), oldValue.d.typeId(), data(), targetTypeId);
    if (!isOk)
        isOk = ::convert(&oldValue.d, targetTypeId, data());

    d.is_null = !isOk;
    return isOk;
}

/*!
  \fn bool QVariant::convert(const int type, void *ptr) const
  \internal
  Created for qvariant_cast() usage
*/
bool QVariant::convert(const int type, void *ptr) const
{
    if (QMetaType::convert(constData(), d.typeId(), ptr, type))
        return true;
    return ::convert(&d, type, ptr);
}


/*!
    \fn bool operator==(const QVariant &v1, const QVariant &v2)

    \relates QVariant

    Returns \c true if \a v1 and \a v2 are equal; otherwise returns \c false.

    If \a v1 and \a v2 have the same \l{QVariant::}{type()}, the
    type's equality operator is used for comparison. If not, it is
    attempted to \l{QVariant::}{convert()} \a v2 to the same type as
    \a v1. See \l{QVariant::}{canConvert()} for a list of possible
    conversions.

    The result of the function is not affected by the result of QVariant::isNull,
    which means that two values can be equal even if one of them is null and
    another is not.
*/

/*!
    \fn bool operator!=(const QVariant &v1, const QVariant &v2)

    \relates QVariant

    Returns \c false if \a v1 and \a v2 are equal; otherwise returns \c true.
*/

/*! \fn bool QVariant::operator==(const QVariant &v) const

    Compares this QVariant with \a v and returns \c true if they are
    equal; otherwise returns \c false.

    QVariant uses the equality operator of the type() it contains to
    check for equality.

    Variants of different types will always compare as not equal with a few
    exceptions:

    \list
    \li If both types are numeric types (integers and floatins point numbers)
    Qt will compare those types using standard C++ type promotion rules.
    \li If one type is numeric and the other one a QString, Qt will try to
    convert the QString to a matching numeric type and if successful compare
    those.
    \endlist
*/

/*!
    \fn bool QVariant::operator!=(const QVariant &v) const

    Compares this QVariant with \a v and returns \c true if they are not
    equal; otherwise returns \c false.

    QVariant uses the equality operator of the type() it contains to
    check for equality.

    Variants of different types will always compare as not equal with a few
    exceptions:

    \list
    \li If both types are numeric types (integers and floatins point numbers)
    Qt will compare those types using standard C++ type promotion rules.
    \li If one type is numeric and the other one a QString, Qt will try to
    convert the QString to a matching numeric type and if successful compare
    those.
    \endlist
*/

static bool qIsNumericType(uint tp)
{
    static const qulonglong numericTypeBits =
            Q_UINT64_C(1) << QMetaType::QString |
            Q_UINT64_C(1) << QMetaType::Bool |
            Q_UINT64_C(1) << QMetaType::Double |
            Q_UINT64_C(1) << QMetaType::Float |
            Q_UINT64_C(1) << QMetaType::Char |
            Q_UINT64_C(1) << QMetaType::SChar |
            Q_UINT64_C(1) << QMetaType::UChar |
            Q_UINT64_C(1) << QMetaType::Short |
            Q_UINT64_C(1) << QMetaType::UShort |
            Q_UINT64_C(1) << QMetaType::Int |
            Q_UINT64_C(1) << QMetaType::UInt |
            Q_UINT64_C(1) << QMetaType::Long |
            Q_UINT64_C(1) << QMetaType::ULong |
            Q_UINT64_C(1) << QMetaType::LongLong |
            Q_UINT64_C(1) << QMetaType::ULongLong;
    return tp < (CHAR_BIT * sizeof numericTypeBits) ? numericTypeBits & (Q_UINT64_C(1) << tp) : false;
}

static bool qIsFloatingPoint(uint tp)
{
    return tp == QMetaType::Double || tp == QMetaType::Float;
}

static int normalizeLowerRanks(uint tp)
{
    static const qulonglong numericTypeBits =
            Q_UINT64_C(1) << QMetaType::Bool |
            Q_UINT64_C(1) << QMetaType::Char |
            Q_UINT64_C(1) << QMetaType::SChar |
            Q_UINT64_C(1) << QMetaType::UChar |
            Q_UINT64_C(1) << QMetaType::Short |
            Q_UINT64_C(1) << QMetaType::UShort;
    return numericTypeBits & (Q_UINT64_C(1) << tp) ? uint(QMetaType::Int) : tp;
}

static int normalizeLong(uint tp)
{
    const uint IntType = sizeof(long) == sizeof(int) ? QMetaType::Int : QMetaType::LongLong;
    const uint UIntType = sizeof(ulong) == sizeof(uint) ? QMetaType::UInt : QMetaType::ULongLong;
    return tp == QMetaType::Long ? IntType :
           tp == QMetaType::ULong ? UIntType : tp;
}

static int numericTypePromotion(uint t1, uint t2)
{
    Q_ASSERT(qIsNumericType(t1));
    Q_ASSERT(qIsNumericType(t2));

    if ((t1 == QMetaType::Bool && t2 == QMetaType::QString) ||
        (t2 == QMetaType::Bool && t1 == QMetaType::QString))
        return QMetaType::Bool;

    // C++ integral ranks: (4.13 Integer conversion rank [conv.rank])
    //   bool < signed char < short < int < long < long long
    //   unsigneds have the same rank as their signed counterparts
    // C++ integral promotion rules (4.5 Integral Promotions [conv.prom])
    // - any type with rank less than int can be converted to int or unsigned int
    // 5 Expressions [expr] paragraph 9:
    // - if either operand is double, the other shall be converted to double
    // -     "       "        float,   "          "         "         float
    // - if both operands have the same type, no further conversion is needed.
    // - if both are signed or if both are unsigned, convert to the one with highest rank
    // - if the unsigned has higher or same rank, convert the signed to the unsigned one
    // - if the signed can represent all values of the unsigned, convert to the signed
    // - otherwise, convert to the unsigned corresponding to the rank of the signed

    // floating point: we deviate from the C++ standard by always using qreal
    if (qIsFloatingPoint(t1) || qIsFloatingPoint(t2))
        return QMetaType::QReal;

    // integral rules:
    // for all platforms we support, int can always hold the values of lower-ranked types
    t1 = normalizeLowerRanks(t1);
    t2 = normalizeLowerRanks(t2);

    // normalize long / ulong: in all platforms we run, they're either the same as int or as long long
    t1 = normalizeLong(t1);
    t2 = normalizeLong(t2);

    // implement the other rules
    // the four possibilities are Int, UInt, LongLong and ULongLong
    // if any of the two is ULongLong, then it wins (highest rank, unsigned)
    // otherwise, if one of the two is LongLong, then the other is either LongLong too or lower-ranked
    // otherwise, if one of the two is UInt, then the other is either UInt too or Int
    if (t1 == QMetaType::ULongLong || t2 == QMetaType::ULongLong)
        return QMetaType::ULongLong;
    if (t1 == QMetaType::LongLong || t2 == QMetaType::LongLong)
        return QMetaType::LongLong;
    if (t1 == QMetaType::UInt || t2 == QMetaType::UInt)
        return QMetaType::UInt;
    return QMetaType::Int;
}

static bool integralEquals(uint promotedType, const QVariant::Private *d1, const QVariant::Private *d2)
{
    // use toLongLong to retrieve the data, it gets us all the bits
    bool ok;
    qlonglong l1 = qConvertToNumber(d1, &ok, promotedType == QMetaType::Bool);
    if (!ok)
        return false;

    qlonglong l2 = qConvertToNumber(d2, &ok, promotedType == QMetaType::Bool);
    if (!ok)
        return false;

    if (promotedType == QMetaType::Bool)
        return bool(l1) == bool(l2);
    if (promotedType == QMetaType::Int)
        return int(l1) == int(l2);
    if (promotedType == QMetaType::UInt)
        return uint(l1) == uint(l2);
    if (promotedType == QMetaType::LongLong)
        return l1 == l2;
    if (promotedType == QMetaType::ULongLong)
        return qulonglong(l1) == qulonglong(l2);

    Q_UNREACHABLE();
    return 0;
}

static bool numericEquals(const QVariant::Private *d1, const QVariant::Private *d2)
{
    uint promotedType = numericTypePromotion(d1->typeId(), d2->typeId());
    if (promotedType != QMetaType::QReal)
        return integralEquals(promotedType, d1, d2);

    // qreal comparisons
    bool ok;
    qreal r1 = qConvertToRealNumber(d1, &ok);
    if (!ok)
        return false;
    qreal r2 = qConvertToRealNumber(d2, &ok);
    if (!ok)
        return false;
    if (r1 == r2)
        return true;

    // only do fuzzy comparisons for finite, non-zero numbers
    int c1 = qFpClassify(r1);
    int c2 = qFpClassify(r2);
    if ((c1 == FP_NORMAL || c1 == FP_SUBNORMAL) && (c2 == FP_NORMAL || c2 == FP_SUBNORMAL)) {
        if (qFuzzyCompare(r1, r2))
            return true;
    }

    return false;
}

/*!
    \internal
 */
bool QVariant::equals(const QVariant &v) const
{
    auto metatype = d.type();

    if (metatype != v.metaType()) {
        // try numeric comparisons, with C++ type promotion rules (no conversion)
        if (qIsNumericType(metatype.id()) && qIsNumericType(v.d.typeId()))
            return numericEquals(&d, &v.d);
        return false;
    }

    // For historical reasons: QVariant() == QVariant()
    if (!metatype.isValid())
        return true;

    return metatype.equals(d.storage(), v.d.storage());
}

/*!
    \fn const void *QVariant::constData() const
    \fn const void* QVariant::data() const
    \internal
 */



/*!
    \internal
*/
void* QVariant::data()
{
    detach();
    // set is_null to false, as the caller is likely to write some data into this variant
    d.is_null = false;
    return const_cast<void *>(constData());
}


/*!
    Returns \c true if this is a null variant, false otherwise.

    A variant is considered null if it contains no initialized value or a null pointer.

    \note This behavior has been changed from Qt 5, where isNull() would also
    return true if the variant contained an object of a builtin type with an isNull()
    method that returned true for that object.

    \sa convert(int)
*/
bool QVariant::isNull() const
{
    if (d.is_null || !metaType().isValid())
        return true;
    if (metaType().flags() & QMetaType::IsPointer)
        return d.get<void *>() == nullptr;
    return false;
}

#ifndef QT_NO_DEBUG_STREAM
QDebug operator<<(QDebug dbg, const QVariant &v)
{
    QDebugStateSaver saver(dbg);
    const uint typeId = v.d.typeId();
    dbg.nospace() << "QVariant(";
    if (typeId != QMetaType::UnknownType) {
        dbg << QMetaType::typeName(typeId) << ", ";
        bool streamed = v.d.type().debugStream(dbg, v.d.storage());
        if (!streamed && v.canConvert<QString>())
            dbg << v.toString();
    } else {
        dbg << "Invalid";
    }
    dbg << ')';
    return dbg;
}

QDebug operator<<(QDebug dbg, const QVariant::Type p)
{
    QDebugStateSaver saver(dbg);
    dbg.nospace() << "QVariant::"
                  << (int(p) != int(QMetaType::UnknownType)
                     ? QMetaType::typeName(p)
                     : "Invalid");
    return dbg;
}
#endif


/*! \fn template<typename T> void QVariant::setValue(T &&value)

    Stores a copy of \a value. If \c{T} is a type that QVariant
    doesn't support, QMetaType is used to store the value. A compile
    error will occur if QMetaType doesn't handle the type.

    Example:

    \snippet code/src_corelib_kernel_qvariant.cpp 4

    \sa value(), fromValue(), canConvert()
 */

/*! \fn template<typename T> void QVariant::setValue(const QVariant &value)

    Copies \a value over this QVariant. It is equivalent to simply
    assigning \a value to this QVariant.
*/

/*! \fn template<typename T> void QVariant::setValue(QVariant &&value)

    Moves \a value over this QVariant. It is equivalent to simply
    move assigning \a value to this QVariant.
*/

/*! \fn template<typename T> T QVariant::value() const

    Returns the stored value converted to the template type \c{T}.
    Call canConvert() to find out whether a type can be converted.
    If the value cannot be converted, a \l{default-constructed value}
    will be returned.

    If the type \c{T} is supported by QVariant, this function behaves
    exactly as toString(), toInt() etc.

    Example:

    \snippet code/src_corelib_kernel_qvariant.cpp 5

    If the QVariant contains a pointer to a type derived from QObject then
    \c{T} may be any QObject type. If the pointer stored in the QVariant can be
    qobject_cast to T, then that result is returned. Otherwise \nullptr is
    returned. Note that this only works for QObject subclasses which use
    the Q_OBJECT macro.

    If the QVariant contains a sequential container and \c{T} is QVariantList, the
    elements of the container will be converted into \l {QVariant}s and returned as a QVariantList.

    \snippet code/src_corelib_kernel_qvariant.cpp 9

    \sa setValue(), fromValue(), canConvert(), Q_DECLARE_SEQUENTIAL_CONTAINER_METATYPE()
*/

/*! \fn bool QVariant::canConvert() const

    Returns \c true if the variant can be converted to the template type \c{T},
    otherwise false.

    Example:

    \snippet code/src_corelib_kernel_qvariant.cpp 6

    A QVariant containing a pointer to a type derived from QObject will also return true for this
    function if a qobject_cast to the template type \c{T} would succeed. Note that this only works
    for QObject subclasses which use the Q_OBJECT macro.

    \sa convert()
*/

/*! \fn template<typename T> static QVariant QVariant::fromValue(const T &value)

    Returns a QVariant containing a copy of \a value. Behaves
    exactly like setValue() otherwise.

    Example:

    \snippet code/src_corelib_kernel_qvariant.cpp 7

    \note If you are working with custom types, you should use
    the Q_DECLARE_METATYPE() macro to register your custom type.

    \sa setValue(), value()
*/

/*! \fn template<typename... Types> QVariant QVariant::fromStdVariant(const std::variant<Types...> &value)
    \since 5.11

    Returns a QVariant with the type and value of the active variant of \a value. If
    the active type is std::monostate a default QVariant is returned.

    \note With this method you do not need to register the variant as a Qt metatype,
    since the std::variant is resolved before being stored. The component types
    should be registered however.

    \sa fromValue()
*/

/*!
    \fn template<typename T> T qvariant_cast(const QVariant &value)
    \relates QVariant

    Returns the given \a value converted to the template type \c{T}.

    This function is equivalent to QVariant::value().

    \sa QVariant::value()
*/

/*! \fn template<typename T> T qVariantValue(const QVariant &value)
    \relates QVariant
    \obsolete

    Returns the given \a value converted to the template type \c{T}.

    This function is equivalent to
    \l{QVariant::value()}{QVariant::value}<T>(\a value).

    \note This function was provided as a workaround for MSVC 6
    which did not support member template functions. It is advised
    to use the other form in new code.

    \sa QVariant::value(), qvariant_cast()
*/

/*! \fn bool qVariantCanConvert(const QVariant &value)
    \relates QVariant
    \obsolete

    Returns \c true if the given \a value can be converted to the
    template type specified; otherwise returns \c false.

    This function is equivalent to QVariant::canConvert(\a value).

    \note This function was provided as a workaround for MSVC 6
    which did not support member template functions. It is advised
    to use the other form in new code.

    \sa QVariant::canConvert()
*/

/*!
    \typedef QVariantList
    \relates QVariant

    Synonym for QList<QVariant>.
*/

/*!
    \typedef QVariantMap
    \relates QVariant

    Synonym for QMap<QString, QVariant>.
*/

/*!
    \typedef QVariantHash
    \relates QVariant
    \since 4.5

    Synonym for QHash<QString, QVariant>.
*/

/*!
    \typedef QVariant::DataPtr
    \internal
*/
/*! \typedef QVariant::f_construct
  \internal
*/

/*! \typedef QVariant::f_clear
  \internal
*/

/*! \typedef QVariant::f_null
  \internal
*/

/*! \typedef QVariant::f_load
  \internal
*/

/*! \typedef QVariant::f_save
  \internal
*/

/*! \typedef QVariant::f_compare
  \internal
*/

/*! \typedef QVariant::f_convert
  \internal
*/

/*! \typedef QVariant::f_canConvert
  \internal
*/

/*! \typedef QVariant::f_debugStream
  \internal
*/

/*!
    \fn DataPtr &QVariant::data_ptr()
    \internal
*/

/*!
    \fn const DataPtr &QVariant::data_ptr() const
    \internal
*/

/*!
    \class QSequentialIterable
    \since 5.2
    \inmodule QtCore
    \brief The QSequentialIterable class is an iterable interface for a container in a QVariant.

    This class allows several methods of accessing the elements of a container held within
    a QVariant. An instance of QSequentialIterable can be extracted from a QVariant if it can
    be converted to a QVariantList.

    \snippet code/src_corelib_kernel_qvariant.cpp 9

    The container itself is not copied before iterating over it.

    \sa QVariant
*/

/*!
    \internal
*/
QSequentialIterable::QSequentialIterable(const QtMetaTypePrivate::QSequentialIterableImpl &impl)
  : m_impl(impl)
{
}

QSequentialIterable::const_iterator::const_iterator(const QSequentialIterable &iter, QAtomicInt *ref_)
  : m_impl(iter.m_impl), ref(ref_)
{
    ref->ref();
}

QSequentialIterable::const_iterator::const_iterator(const QtMetaTypePrivate::QSequentialIterableImpl &impl, QAtomicInt *ref_)
  : m_impl(impl), ref(ref_)
{
    ref->ref();
}

void QSequentialIterable::const_iterator::begin()
{
    m_impl.moveToBegin();
}

void QSequentialIterable::const_iterator::end()
{
    m_impl.moveToEnd();
}

/*! \fn QSequentialIterable::const_iterator QSequentialIterable::begin() const

    Returns a QSequentialIterable::const_iterator for the beginning of the container. This
    can be used in stl-style iteration.

    \sa end()
*/
QSequentialIterable::const_iterator QSequentialIterable::begin() const
{
    const_iterator it(*this, new QAtomicInt(0));
    it.begin();
    return it;
}

/*!
    Returns a QSequentialIterable::const_iterator for the end of the container. This
    can be used in stl-style iteration.

    \sa begin()
*/
QSequentialIterable::const_iterator QSequentialIterable::end() const
{
    const_iterator it(*this, new QAtomicInt(0));
    it.end();
    return it;
}

/*!
    Returns the element at position \a idx in the container.
*/
QVariant QSequentialIterable::at(int idx) const
{
    QVariant v(m_impl._metaType);
    void *dataPtr;
    if (m_impl._metaType == QMetaType::fromType<QVariant>())
        dataPtr = &v;
    else
        dataPtr = v.data();
    m_impl.at(idx, dataPtr);
    return v;
}

/*!
    Returns the number of elements in the container.
*/
int QSequentialIterable::size() const
{
    return m_impl.size();
}

/*!
    Returns whether it is possible to iterate over the container in reverse. This
    corresponds to the std::bidirectional_iterator_tag iterator trait of the
    const_iterator of the container.
*/
bool QSequentialIterable::canReverseIterate() const
{
    return m_impl._iteratorCapabilities & QtMetaTypePrivate::BiDirectionalCapability;
}

/*!
    \class QSequentialIterable::const_iterator
    \since 5.2
    \inmodule QtCore
    \brief The QSequentialIterable::const_iterator allows iteration over a container in a QVariant.

    A QSequentialIterable::const_iterator can only be created by a QSequentialIterable instance,
    and can be used in a way similar to other stl-style iterators.

    \snippet code/src_corelib_kernel_qvariant.cpp 9

    \sa QSequentialIterable
*/


/*!
    Destroys the QSequentialIterable::const_iterator.
*/
QSequentialIterable::const_iterator::~const_iterator() {
    if (!ref->deref()) {
        m_impl.destroyIter();
        delete ref;
    }
}

/*!
    Creates a copy of \a other.
*/
QSequentialIterable::const_iterator::const_iterator(const const_iterator &other)
  : m_impl(other.m_impl), ref(other.ref)
{
    ref->ref();
}

/*!
    Assigns \a other to this.
*/
QSequentialIterable::const_iterator&
QSequentialIterable::const_iterator::operator=(const const_iterator &other)
{
    other.ref->ref();
    if (!ref->deref()) {
        m_impl.destroyIter();
        delete ref;
    }
    m_impl = other.m_impl;
    ref = other.ref;
    return *this;
}

/*!
    Returns the current item, converted to a QVariant.
*/
const QVariant QSequentialIterable::const_iterator::operator*() const
{
    QVariant v(m_impl._metaType);
    void *dataPtr;
    if (m_impl._metaType == QMetaType::fromType<QVariant>())
        dataPtr = &v;
    else
        dataPtr = v.data();
    m_impl.getCurrent(dataPtr);
    return v;
}

/*!
    Returns \c true if \a other points to the same item as this
    iterator; otherwise returns \c false.

    \sa operator!=()
*/
bool QSequentialIterable::const_iterator::operator==(const const_iterator &other) const
{
    return m_impl.equal(other.m_impl);
}

/*!
    Returns \c true if \a other points to a different item than this
    iterator; otherwise returns \c false.

    \sa operator==()
*/
bool QSequentialIterable::const_iterator::operator!=(const const_iterator &other) const
{
    return !m_impl.equal(other.m_impl);
}

/*!
    The prefix ++ operator (\c{++it}) advances the iterator to the
    next item in the container and returns an iterator to the new current
    item.

    Calling this function on QSequentialIterable::end() leads to undefined results.

    \sa operator--()
*/
QSequentialIterable::const_iterator &QSequentialIterable::const_iterator::operator++()
{
    m_impl.advance(1);
    return *this;
}

/*!
    \overload

    The postfix ++ operator (\c{it++}) advances the iterator to the
    next item in the container and returns an iterator to the previously
    current item.
*/
QSequentialIterable::const_iterator QSequentialIterable::const_iterator::operator++(int)
{
    QtMetaTypePrivate::QSequentialIterableImpl impl;
    impl.copy(m_impl);
    m_impl.advance(1);
    return const_iterator(impl, new QAtomicInt(0));
}

/*!
    The prefix -- operator (\c{--it}) makes the preceding item
    current and returns an iterator to the new current item.

    Calling this function on QSequentialIterable::begin() leads to undefined results.

    If the container in the QVariant does not support bi-directional iteration, calling this function
    leads to undefined results.

    \sa operator++(), canReverseIterate()
*/
QSequentialIterable::const_iterator &QSequentialIterable::const_iterator::operator--()
{
    m_impl.advance(-1);
    return *this;
}

/*!
    \overload

    The postfix -- operator (\c{it--}) makes the preceding item
    current and returns an iterator to the previously current item.

    If the container in the QVariant does not support bi-directional iteration, calling this function
    leads to undefined results.

    \sa canReverseIterate()
*/
QSequentialIterable::const_iterator QSequentialIterable::const_iterator::operator--(int)
{
    QtMetaTypePrivate::QSequentialIterableImpl impl;
    impl.copy(m_impl);
    m_impl.advance(-1);
    return const_iterator(impl, new QAtomicInt(0));
}

/*!
    Advances the iterator by \a j items.

    \sa operator-=(), operator+()
*/
QSequentialIterable::const_iterator &QSequentialIterable::const_iterator::operator+=(int j)
{
    m_impl.advance(j);
    return *this;
}

/*!
    Makes the iterator go back by \a j items.

    If the container in the QVariant does not support bi-directional iteration, calling this function
    leads to undefined results.

    \sa operator+=(), operator-(), canReverseIterate()
*/
QSequentialIterable::const_iterator &QSequentialIterable::const_iterator::operator-=(int j)
{
    m_impl.advance(-j);
    return *this;
}

/*!
    Returns an iterator to the item at \a j positions forward from
    this iterator.

    \sa operator-(), operator+=()
*/
QSequentialIterable::const_iterator QSequentialIterable::const_iterator::operator+(int j) const
{
    QtMetaTypePrivate::QSequentialIterableImpl impl;
    impl.copy(m_impl);
    impl.advance(j);
    return const_iterator(impl, new QAtomicInt(0));
}

/*!
    Returns an iterator to the item at \a j positions backward from
    this iterator.

    If the container in the QVariant does not support bi-directional iteration, calling this function
    leads to undefined results.

    \sa operator+(), operator-=(), canReverseIterate()
*/
QSequentialIterable::const_iterator QSequentialIterable::const_iterator::operator-(int j) const
{
    QtMetaTypePrivate::QSequentialIterableImpl impl;
    impl.copy(m_impl);
    impl.advance(-j);
    return const_iterator(impl, new QAtomicInt(0));
}

/*!
    \class QAssociativeIterable
    \since 5.2
    \inmodule QtCore
    \brief The QAssociativeIterable class is an iterable interface for an associative container in a QVariant.

    This class allows several methods of accessing the elements of an associative container held within
    a QVariant. An instance of QAssociativeIterable can be extracted from a QVariant if it can
    be converted to a QVariantHash or QVariantMap.

    \snippet code/src_corelib_kernel_qvariant.cpp 10

    The container itself is not copied before iterating over it.

    \sa QVariant
*/

/*!
    \internal
*/
QAssociativeIterable::QAssociativeIterable(const QtMetaTypePrivate::QAssociativeIterableImpl &impl)
  : m_impl(impl)
{
}

QAssociativeIterable::const_iterator::const_iterator(const QAssociativeIterable &iter, QAtomicInt *ref_)
  : m_impl(iter.m_impl), ref(ref_)
{
    ref->ref();
}

QAssociativeIterable::const_iterator::const_iterator(const QtMetaTypePrivate::QAssociativeIterableImpl &impl, QAtomicInt *ref_)
  : m_impl(impl), ref(ref_)
{
    ref->ref();
}

void QAssociativeIterable::const_iterator::begin()
{
    m_impl.begin();
}

void QAssociativeIterable::const_iterator::end()
{
    m_impl.end();
}

void QAssociativeIterable::const_iterator::find(const QVariant &key)
{
    Q_ASSERT(key.metaType() == m_impl._metaType_key);
    m_impl.find(key.constData());
}

/*!
    Returns a QAssociativeIterable::const_iterator for the beginning of the container. This
    can be used in stl-style iteration.

    \sa end()
*/
QAssociativeIterable::const_iterator QAssociativeIterable::begin() const
{
    const_iterator it(*this, new QAtomicInt(0));
    it.begin();
    return it;
}

/*!
    Returns a QAssociativeIterable::const_iterator for the end of the container. This
    can be used in stl-style iteration.

    \sa begin()
*/
QAssociativeIterable::const_iterator QAssociativeIterable::end() const
{
    const_iterator it(*this, new QAtomicInt(0));
    it.end();
    return it;
}

/*!
    \since 5.5

    Returns a QAssociativeIterable::const_iterator for the given key \a key
    in the container, if the types are convertible.

    If the key is not found, returns end().

    This can be used in stl-style iteration.

    \sa begin(), end(), value()
*/
QAssociativeIterable::const_iterator QAssociativeIterable::find(const QVariant &key) const
{
    const_iterator it(*this, new QAtomicInt(0));
    QVariant key_ = key;
    if (key_.canConvert(m_impl._metaType_key.id()) && key_.convert(m_impl._metaType_key.id()))
        it.find(key_);
    else
        it.end();
    return it;
}

/*!
    Returns the value for the given \a key in the container, if the types are convertible.

    \sa find()
*/
QVariant QAssociativeIterable::value(const QVariant &key) const
{
    const const_iterator it = find(key);
    if (it == end())
        return QVariant();
    return *it;
}

/*!
    Returns the number of elements in the container.
*/
int QAssociativeIterable::size() const
{
    return m_impl.size();
}

/*!
    \class QAssociativeIterable::const_iterator
    \since 5.2
    \inmodule QtCore
    \brief The QAssociativeIterable::const_iterator allows iteration over a container in a QVariant.

    A QAssociativeIterable::const_iterator can only be created by a QAssociativeIterable instance,
    and can be used in a way similar to other stl-style iterators.

    \snippet code/src_corelib_kernel_qvariant.cpp 10

    \sa QAssociativeIterable
*/


/*!
    Destroys the QAssociativeIterable::const_iterator.
*/
QAssociativeIterable::const_iterator::~const_iterator()
{
    if (!ref->deref()) {
        m_impl.destroyIter();
        delete ref;
    }
}

/*!
    Creates a copy of \a other.
*/
QAssociativeIterable::const_iterator::const_iterator(const const_iterator &other)
  : m_impl(other.m_impl), ref(other.ref)
{
    ref->ref();
}

/*!
    Assigns \a other to this.
*/
QAssociativeIterable::const_iterator&
QAssociativeIterable::const_iterator::operator=(const const_iterator &other)
{
    other.ref->ref();
    if (!ref->deref()) {
        m_impl.destroyIter();
        delete ref;
    }
    m_impl = other.m_impl;
    ref = other.ref;
    return *this;
}

/*!
    Returns the current value, converted to a QVariant.
*/
const QVariant QAssociativeIterable::const_iterator::operator*() const
{
    QVariant v(m_impl._metaType_value);
    void *dataPtr;
    if (m_impl._metaType_value == QMetaType::fromType<QVariant>())
        dataPtr = &v;
    else
        dataPtr = v.data();
    m_impl.getCurrentValue(dataPtr);
    return v;
}

/*!
    Returns the current key, converted to a QVariant.
*/
const QVariant QAssociativeIterable::const_iterator::key() const
{
    QVariant v(m_impl._metaType_key);
    void *dataPtr;
    if (m_impl._metaType_key == QMetaType::fromType<QVariant>())
        dataPtr = &v;
    else
        dataPtr = v.data();
    m_impl.getCurrentKey(dataPtr);
    return v;
}

/*!
    Returns the current value, converted to a QVariant.
*/
const QVariant QAssociativeIterable::const_iterator::value() const
{
    return operator*();
}

/*!
    Returns \c true if \a other points to the same item as this
    iterator; otherwise returns \c false.

    \sa operator!=()
*/
bool QAssociativeIterable::const_iterator::operator==(const const_iterator &other) const
{
    return m_impl.equal(other.m_impl);
}

/*!
    Returns \c true if \a other points to a different item than this
    iterator; otherwise returns \c false.

    \sa operator==()
*/
bool QAssociativeIterable::const_iterator::operator!=(const const_iterator &other) const
{
    return !m_impl.equal(other.m_impl);
}

/*!
    The prefix ++ operator (\c{++it}) advances the iterator to the
    next item in the container and returns an iterator to the new current
    item.

    Calling this function on QAssociativeIterable::end() leads to undefined results.

    \sa operator--()
*/
QAssociativeIterable::const_iterator &QAssociativeIterable::const_iterator::operator++()
{
    m_impl.advance(1);
    return *this;
}

/*!
    \overload

    The postfix ++ operator (\c{it++}) advances the iterator to the
    next item in the container and returns an iterator to the previously
    current item.
*/
QAssociativeIterable::const_iterator QAssociativeIterable::const_iterator::operator++(int)
{
    QtMetaTypePrivate::QAssociativeIterableImpl impl;
    impl.copy(m_impl);
    m_impl.advance(1);
    return const_iterator(impl, new QAtomicInt(0));
}

/*!
    The prefix -- operator (\c{--it}) makes the preceding item
    current and returns an iterator to the new current item.

    Calling this function on QAssociativeIterable::begin() leads to undefined results.

    \sa operator++()
*/
QAssociativeIterable::const_iterator &QAssociativeIterable::const_iterator::operator--()
{
    m_impl.advance(-1);
    return *this;
}

/*!
    \overload

    The postfix -- operator (\c{it--}) makes the preceding item
    current and returns an iterator to the previously current item.
*/
QAssociativeIterable::const_iterator QAssociativeIterable::const_iterator::operator--(int)
{
    QtMetaTypePrivate::QAssociativeIterableImpl impl;
    impl.copy(m_impl);
    m_impl.advance(-1);
    return const_iterator(impl, new QAtomicInt(0));
}

/*!
    Advances the iterator by \a j items.

    \sa operator-=(), operator+()
*/
QAssociativeIterable::const_iterator &QAssociativeIterable::const_iterator::operator+=(int j)
{
    m_impl.advance(j);
    return *this;
}

/*!
    Makes the iterator go back by \a j items.

    \sa operator+=(), operator-()
*/
QAssociativeIterable::const_iterator &QAssociativeIterable::const_iterator::operator-=(int j)
{
    m_impl.advance(-j);
    return *this;
}

/*!
    Returns an iterator to the item at \a j positions forward from
    this iterator.

    \sa operator-(), operator+=()
*/
QAssociativeIterable::const_iterator QAssociativeIterable::const_iterator::operator+(int j) const
{
    QtMetaTypePrivate::QAssociativeIterableImpl impl;
    impl.copy(m_impl);
    impl.advance(j);
    return const_iterator(impl, new QAtomicInt(0));
}

/*!
    Returns an iterator to the item at \a j positions backward from
    this iterator.

    \sa operator+(), operator-=()
*/
QAssociativeIterable::const_iterator QAssociativeIterable::const_iterator::operator-(int j) const
{
    QtMetaTypePrivate::QAssociativeIterableImpl impl;
    impl.copy(m_impl);
    impl.advance(-j);
    return const_iterator(impl, new QAtomicInt(0));
}

QVariant::Private::~Private()
{
    QtMetaTypePrivate::derefAndDestroy(
            reinterpret_cast<QtPrivate::QMetaTypeInterface *>(packedType << 2));
}

QT_END_NAMESPACE
