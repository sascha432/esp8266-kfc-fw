/**
* Author: sascha_lammers@gmx.de
*/

#include "TableDumper.h"
#include <algorithm>
#if DEBUG
#include <assert.h>
#endif

#if HAVE_PROGMEM_DATA

#include "progmem_data.h"

PROGMEM_STRING_DECL(true);
#define _shared_progmem_string_TableDumper_STR_BOOL_true _shared_progmem_string_true
PROGMEM_STRING_DECL(false);
#define _shared_progmem_string_TableDumper_STR_BOOL_false _shared_progmem_string_false
PROGMEM_STRING_DECL(1);
#define _shared_progmem_string_TableDumper_BOOL_true _shared_progmem_string_1
PROGMEM_STRING_DECL(0);
#define _shared_progmem_string_TableDumper_BOOL_false _shared_progmem_string_0

#else

PROGMEM_STRING_DEF(TableDumper_STR_BOOL_true, "true");
PROGMEM_STRING_DEF(TableDumper_STR_BOOL_false, "false");
PROGMEM_STRING_DEF(TableDumper_BOOL_true, "1");
PROGMEM_STRING_DEF(TableDumper_BOOL_false, "0");

#endif

PROGMEM_STRING_DEF(TableDumper_STR_nullptr, "null");
PROGMEM_STRING_DEF(TableDumper_FLOAT_format, "%.2f");


TableDumper *TableDumper::_instance;


TableDumperColumn::TableDumperColumn(const String &str) {
    _data.str = new String(str);
    _type = STRING;
}

TableDumperColumn::TableDumperColumn(const char * str) {
    _data.c_str = str;
    _type = C_STR;
}

TableDumperColumn::TableDumperColumn(const __FlashStringHelper * str) {
    _data.f_str = str;
    _type = F_STR;
}

TableDumperColumn::TableDumperColumn(char value) {
    _data._pointer = nullptr;
    _data._char = value;
    _type = SCHAR;
}

TableDumperColumn::TableDumperColumn(unsigned char value) {
    _data._pointer = nullptr;
    _data._uchar = value;
    _type = UCHAR;
}

TableDumperColumn::TableDumperColumn(int value) {
    _data._int = value;
    _type = INT;
}

TableDumperColumn::TableDumperColumn(unsigned value) {
    _data._uint = value;
    _type = UINT;
}

TableDumperColumn::TableDumperColumn(long value) {
    _data._long = value;
    _type = LONG;
}

TableDumperColumn::TableDumperColumn(unsigned long value) {
    _data._ulong = value;
    _type = ULONG;
}

TableDumperColumn::TableDumperColumn(float value) {
    _data._float = value;
    _type = FLOAT;

}

TableDumperColumn::TableDumperColumn(const void * ptr) {
    _data._pointer = ptr;
    _type = POINTER;
}

TableDumperColumn::TableDumperColumn(bool value, Type_t type) {
    _data._bool = value;
    _type = type;
}

void TableDumperColumn::freeData() {
    if (_type == STRING && _data.str) {
        delete _data.str;
        _data.str = nullptr;
    }
}

void TableDumperColumn::dump(Print & output, Length_t width, Align_t align) const {
    String str = _getString();
    if ((Length_t)str.length() > width) {
        str.remove(width);
    }
    uint8_t leftFill;
    uint8_t rightFill;

    auto length = (Length_t)str.length();
    rightFill = width - length;
    if (align == RIGHT) {
        // swap
        leftFill = rightFill;
        rightFill = 0;
    }
    else if (align == LEFT) {
        leftFill = 0;
    }
    else {
        leftFill = rightFill / 2;
        rightFill = width - leftFill - length;
    }

    while (leftFill--) {
        output.print(' ');
    }

    output.print(str);

    while (rightFill--) {
        output.print(' ');
    }
}

TableDumperColumn::Length_t TableDumperColumn::getLength() const {
    switch (_type) {
    case C_STR:
        if (!_data.c_str) {
            return (Length_t)TableDumper::getInstance()->getStrNullValue().length();
        }
        return (Length_t)strlen(_data.c_str);
    case F_STR:
        if (!_data.f_str) {
            return (Length_t)TableDumper::getInstance()->getStrNullValue().length();
        }
        return (Length_t)strlen_P(reinterpret_cast<PGM_P>(_data.f_str));
    case STR_BOOL:
        return (Length_t)TableDumper::getInstance()->getStrBoolValue(_data._bool).length();
    case BOOL:
        return (Length_t)strlen_P(_data._bool ? SPGM(TableDumper_BOOL_true) : SPGM(TableDumper_BOOL_false));
    case SCHAR:
        return _valueLength(_data._char);
    case INT:
        return _valueLength(_data._int);
    case LONG:
        return _valueLength(_data._long);
    case UCHAR:
        return _unsignedValueLength(_data._uchar);
    case UINT:
        return _unsignedValueLength(_data._uint);
    case ULONG:
        return _unsignedValueLength(_data._ulong);
    default:
        break;
    }
    return (Length_t)_getString().length();
}

String TableDumperColumn::_getString() const {
    switch (_type) {
    case STRING:
        if (!_data.str) {
            return TableDumper::getInstance()->getStrNullValue();
        }
        return *(String *)_data.str;
    case C_STR:
        if (!_data.c_str) {
            return TableDumper::getInstance()->getStrNullValue();
        }
        return String(_data.c_str);
    case F_STR:
        if (!_data.f_str) {
            return TableDumper::getInstance()->getStrNullValue();
        }
        return String(FPSTR(_data.f_str));
    case SCHAR:
        String(_data._char);
    case UCHAR:
        String(_data._uchar);
    case INT:
        return String(_data._int);
    case UINT:
        return String(_data._uint);
    case LONG:
        return String(_data._long);
    case ULONG:
        return String(_data._ulong);
    case FLOAT: {
            char buf[TABLE_DUMPER_FLOAT_MAX_DIGITS];
            snprintf(buf, sizeof(buf), TableDumper::getInstance()->getFloatFormat().c_str(), _data._float);
            return String(buf);
        }
    case STR_BOOL:
        return TableDumper::getInstance()->getStrBoolValue(_data._bool);
    case BOOL:
        return _data._bool ? SPGM(TableDumper_BOOL_true) : SPGM(TableDumper_BOOL_false);
    case POINTER: {
            PrintString str(F("%p"), _data._pointer);
            return str;
        }
    }
    return _sharedEmptyString;
}

TableDumperColumn::Length_t TableDumperColumn::_valueLength(long value) const {
    return (value < 0) ? (_unsignedValueLength(-value) + 1) : _unsignedValueLength(value);
}

TableDumperColumn::Length_t TableDumperColumn::_unsignedValueLength(unsigned long value) const {
    if (value) {
        Length_t len = 0;
        while (value) {
            value /= 10;
            len++;
        }
        return len;
    }
    return 1;
}

TableDumper::TableDumper(Print &output, bool autoWidth) : _output(output) {
    _autoCalc = autoWidth;
    _hasTitles = false;
    _boolValues[0] = SPGM(TableDumper_STR_BOOL_true);
    _boolValues[1] = SPGM(TableDumper_STR_BOOL_false);
    _strValueNull = SPGM(TableDumper_STR_nullptr);
    _floatFormat = SPGM(TableDumper_FLOAT_format);
}

TableDumper::~TableDumper() {
    _free();
}

TableDumper &TableDumper::addColumn(Length_t width, Align_t align) {
    return addColumn(TableDumperColumn(_sharedEmptyString), width, align);
}

TableDumper & TableDumper::_addColumn(const TableDumperColumn &title, Length_t width, Align_t align) {
    if (width == AUTO_WIDTH) {
        _autoCalc = true;
    }
    _columnInfos.push_back({title, width, align});
    return *this;
}

void TableDumper::clear() {
    _free();
    _columnInfos.clear();
    _rows.clear();
    _autoCalc = false;
    _hasTitles = false;
}

TableDumper &TableDumper::startTable() {
    _instance = this;
    if (_autoCalc) {
        for (auto &col: _columnInfos) {
            col.width = std::max(col.width, col.title.getLength());
        }
    } else {
        _headers();
        _currentCol = __first();
    }
    return *this;
}

TableDumper & TableDumper::_addData(const TableDumperColumn &col) {
    if (_autoCalc) {
        if (_currentCol == __first()) {
            _rows.push_back(ColumnVector());
        }
        _columnInfos[_currentCol].width = std::max(_columnInfos[_currentCol].width, col.getLength());

        _rows.back().push_back(col);
        if (++_currentCol == __last()) {
            _endRow();
        }
    }
    else {
        if (_currentCol == __first()) {
            _dumpColumBorder();
        }
#if DEBUG
        assert((int)_currentCol <= (int)_columnInfos.size());
#endif
        col.dump(_output, _columnInfos[_currentCol - 1].width, _columnInfos[_currentCol - 1].align);
        _dumpColumBorder();
    }
    if (_currentCol == __last() + 1) {
        _endRow();
    }
    return *this;
}

void TableDumper::_endRow() {
    _currentCol = __first();
}

void TableDumper::endTable() {
    if (_autoCalc) {
        _headers();
        for (auto &row : _rows) {
            _dumpColumns(row);
        }
        _footers();
    }
    else {
        _footers();
    }
    _instance = nullptr;
}

void TableDumper::_headers() {
    _currentCol = __first();
    for (auto &col : _columnInfos) {
        _header(col);
    }
    if (_hasTitles) {
        _currentCol = __first();
        _dumpColumBorder();
        for (auto &col : _columnInfos) {
            col.title.dump(_output, col.width, DEFAULT_ALIGNMENT);
            col.title.freeData();
            _dumpColumBorder();
        }
        _subHeaders();
    }
    _currentCol = __first();
}

void TableDumper::_subHeaders() {
    _currentCol = __first();
    for (auto &col : _columnInfos) {
        _subHeader(col);
    }
}

void TableDumper::_footers() {
    _currentCol = __first();
    for (const auto &col : _columnInfos) {
        _footer(col);
    }
}

void TableDumper::_dumpColumns(ColumnVector &row) {
    _currentCol = __first();
    _dumpColumBorder();
    for (auto &col : row) {
#if DEBUG
        assert((int)_currentCol <= (int)_columnInfos.size());
#endif
        col.dump(_output, _columnInfos[_currentCol - 1].width, _columnInfos[_currentCol - 1].align);
        col.freeData();
        _dumpColumBorder();
    }
}

void TableDumper::_header(const Col_t & col) {
    if (_currentCol++ == __first()) {
        _output.print('+');
    }
    auto width = col.width + 2;
    while (width--) {
        _output.print('-');
    }
    _output.print('+');
    if (_currentCol == __last()) {
        _output.println();
    }
}

void TableDumper::_subHeader(const Col_t & col) {
    _header(col);
}

void TableDumper::_footer(const Col_t & col) {
    _header(col);
}

void TableDumper::_dumpColumBorder() {
    if (_currentCol++ == __first()) {
        _output.print('+');
        _output.print(' ');
    } else {
        if (_currentCol == __last() + 1) {
            _output.print(' ');
            _output.print('+');
            _output.println();
        }
        else {
            _output.print(' ');
            _output.print('|');
            _output.print(' ');
        }
    }
}

void TableDumper::_free() {
    for (auto &col : _columnInfos) {
        col.title.freeData();
    }
    for (auto &row : _rows) {
        for (auto &col : row) {
            col.freeData();
        }
    }
    _columnInfos.clear();
    _rows.clear();
}
