/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include "PrintString.h"
#include "misc.h"

#define TABLE_DUMPER_FLOAT_MAX_DIGITS 32

class TableDumper;

//template<typename T> //TODO make this a template too
class TableDumperColumn {
public:
    typedef int8_t Length_t;

    typedef enum {
        MIDDLE = 0,
        CENTER = 0,
        LEFT,
        RIGHT
    } Align_t;

    typedef enum {
        STRING,
        POINTER,
        C_STR,
        F_STR,
        SCHAR,
        UCHAR,
        INT,
        UINT,
        LONG,
        ULONG,
        FLOAT,
        STR_BOOL,
        BOOL,
        // OBJECT_PTR
    } Type_t;

    typedef union {
        String *str;
        const char *c_str;
        const __FlashStringHelper *f_str;
        char _char;
        unsigned char _uchar;
        int _int;
        unsigned _uint;
        long _long;
        unsigned long _ulong;
        float _float;
        bool _bool;
        const void *_pointer;
        //T *_object;
    } Data_t;

    //TableDumperColumn(const T *object) {
    //    _type = OBJECT_PTR;
    //    _data._object = object;
    //}
    TableDumperColumn(const String &str);
    TableDumperColumn(const char *str);
    TableDumperColumn(const __FlashStringHelper *str);
    TableDumperColumn(char value);
    TableDumperColumn(unsigned char value);
    TableDumperColumn(int value);
    TableDumperColumn(unsigned value);
    TableDumperColumn(long value);
    TableDumperColumn(unsigned long value);
    TableDumperColumn(float value);
    TableDumperColumn(const void *ptr);
    TableDumperColumn(bool value, Type_t type = BOOL);
    void freeData();

    void dump(Print &output, Length_t width, Align_t align) const;

    Length_t getLength() const;

private:
    String _getString() const; //TODO make const String
    Length_t _valueLength(long value) const;
    Length_t _unsignedValueLength(unsigned long value) const;

private:
    Data_t _data;
    Type_t _type;
};

class TableDumper {
public:
    typedef TableDumperColumn::Length_t Length_t;
    typedef TableDumperColumn::Align_t Align_t;
    static const Length_t AUTO_WIDTH = -1;

    static const Align_t DEFAULT_ALIGNMENT = TableDumperColumn::LEFT;

    typedef struct {
        TableDumperColumn title;
        Length_t width;
        Align_t align;
    } Col_t;

    typedef enum {
        FIRST = 0,
        NEXT,
        LAST
    } Border_t;

    typedef std::vector<TableDumperColumn> ColumnVector;
    typedef std::vector<Col_t> ColumnInfoVector;
    typedef std::vector<ColumnVector> RowVector;

    // NOTE auto width needs to create the entire table and keep it in memory before it can be dumped
    TableDumper(Print &output, bool autoWidth = false);
    virtual ~TableDumper();

    void setAutoWidth() {
        _autoCalc = true;
    }

    void clear();

    // width is the maximum for this column. if auto width is used or globally set, it becomes min. width
    TableDumper &addColumn(Length_t width, Align_t align = DEFAULT_ALIGNMENT);

    TableDumper &_addColumn(const TableDumperColumn &title, Length_t width, Align_t align = DEFAULT_ALIGNMENT);
    template <typename T>
    TableDumper &addColumn(const T &title, Length_t width = 0, Align_t align = DEFAULT_ALIGNMENT) {
        auto tmp = TableDumperColumn(title);
        _hasTitles = true;
        return _addColumn(tmp, width, align);
    }
    template <typename T>
    TableDumper &addColumn(const T *title, Length_t width = 0, Align_t align = DEFAULT_ALIGNMENT) {
        auto tmp = TableDumperColumn(title);
        _hasTitles = true;
        return _addColumn(tmp, width, align);
    }

    TableDumper &startTable();
    void endTable();

    // String is creating a copy in memory while all other types are passed by reference
    TableDumper &_addData(const TableDumperColumn &col);

    // values for true and false are TableDumperColumn::_boolValueTrue / _boolValueFalse
    TableDumper &addStrBoolData(bool value) {
        return _addData(TableDumperColumn(value, TableDumperColumn::STR_BOOL));
    }

    template <typename T>
    TableDumper &addData(const T &col) {
        return _addData(col);
    }
    template <typename T>
    TableDumper &addData(const T *col) {
        return _addData(col);
    }

private:
    void _headers();
    void _subHeaders();
    void _footers();
    void _dumpColumns(ColumnVector &row);
    void _endRow();

    virtual void _header(const Col_t &col);
    virtual void _subHeader(const Col_t &col);
    virtual void _footer(const Col_t &col);
    virtual void _dumpColumBorder();

    void _free();

    inline const Length_t __first() const {
        return 0;
    }
    inline const Length_t __last() const {
        return (Length_t)_columnInfos.size();
    }

public:
    void setStrBoolValues(const String &trueStr, const String &falseStr) {
        _boolValues[0] = falseStr;
        _boolValues[1] = trueStr;
    }
    const String &getStrBoolValue(bool value) const {
        return _boolValues[value ? 1 : 0];
    }

    void setStrNullValue(const String &str) {
        _strValueNull = str;
    }
    const String &getStrNullValue() const  {
        return _strValueNull;
    }

    void setFloatFormat(const String &format) {
        _floatFormat = format;
    }
    const String &getFloatFormat() const {
        return _floatFormat;
    }

    static const TableDumper *getInstance() {
        return _instance;
    }

private:
    static TableDumper *_instance;

    Print &_output;
    ColumnInfoVector _columnInfos;
    RowVector _rows;
    bool _autoCalc;
    bool _hasTitles;
    int16_t _currentCol;

    String _boolValues[2];
    String _strValueNull;
    String _floatFormat;
};
