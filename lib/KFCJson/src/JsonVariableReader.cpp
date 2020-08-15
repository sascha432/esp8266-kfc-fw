/**
* Author: sascha_lammers@gmx.de
*/

#include "JsonVariableReader.h"

namespace JsonVariableReader {

    Element::Element(const JsonString &path, AssignCallback callback) : _path(path), _callback(callback)
    {
    }

    JsonString &Element::getPath()
    {
        return _path;
    }

    bool Element::callback(Result &result, Reader &reader)
    {
        if (_callback) {
            return _callback(result, reader);
        }
        return true;
    }


    ElementGroup::ElementGroup(const JsonString &path) : _path(path)
    {
    }

    ElementGroup::~ElementGroup()
    {
        for (auto element : _elements) {
            delete element;
        }
        for (auto result : _results) {
            delete result;
        }
    }

    ElementGroup::ElementPtr ElementGroup::add(const JsonString &path, Element::AssignCallback callback)
    {
        _elements.emplace_back(new Element(path, callback));
        return _elements.back();
    }

    ElementGroup::ElementPtr ElementGroup::add(Element *var)
    {
        _elements.emplace_back(var);
        return var;
    }

    bool ElementGroup::isPath(const String &path)
    {
        return _path.equals(path);
    }

    ElementGroup::ElementPtr ElementGroup::findPath(const String &path)
    {
        for (auto &var : _elements) {
            if (var->getPath().equals(path)) {
                return var;
            }
        }
        return nullptr;
    }

    ElementGroup::ResultPtr ElementGroup::getLastResult()
    {
        __LDBG_assert(_results.size() != 0); // initResultType() must be used when creating the object
        return _results.back();
    }

    void ElementGroup::flushResult()
    {
        auto result = getLastResult()->create();
        if (result) {
            _results.emplace_back(result);
        }
    }


    Reader::Reader() : JsonBaseReader(nullptr), _elementGroups(new ElementGroup::Vector()), _current(nullptr), _level(0), _skip(false)
    {
    }

    Reader::~Reader()
    {
        delete _elementGroups;
    }

    ElementGroup::Vector *Reader::getElementGroups()
    {
        return _elementGroups;
    }

    bool Reader::beginObject(bool isArray)
    {
        //auto pathStr = getObjectPath(false);
        //auto path = pathStr.c_str();
        //Serial.printf("begin path level %u %s array=%u\n", getLevel(), path, isArray);
        auto level = getLevel();
        if (!_current) {
            for (auto &group : *_elementGroups) {
                if (group.isPath(getObjectPath(false))) {
                    _current = &group;
                    _level = level;
                    //Serial.printf(" new current %s level %u", _current->getPath().toString().c_str(), _level);
                }
            }
        }
        //Serial.println();
        return true;
    }

    bool Reader::endObject()
    {
        //Serial.printf("end level %u\n", getLevel());
        if (_current) {
            if (_level == getLevel()) {
                if (_skip) {
                    _skip = false;
                    _current->flushResult();
                }
                else {
                    _current->flushResult();
                }
                //Serial.printf("new element skip %u\n", _skip);
            }
            else if (_level - 1 == getLevel()) {
                _current = nullptr;
                //Serial.println("end elements");
            }
        }
        return true;
    }

    bool Reader::processElement()
    {
        //auto keyStr = getKey();
        //auto key = keyStr.c_str();
        //auto pathStr = getPath(false);
        //auto path = pathStr.c_str();
        //Serial.printf("key %s value %s type %s path %s index %d\n", key, getValue().c_str(), jsonType2String(getType()).c_str(), path, getObjectIndex());

        if (_current && !_skip) {
            auto var = _current->findPath(getPath(false, _level));
            if (var) {
                _skip = !var->callback(*_current->getLastResult(), *this);
            }
        }
        return true;
    }

    bool Reader::recoverableError(JsonErrorEnum_t errorType)
    {
        return true;
    }

};
