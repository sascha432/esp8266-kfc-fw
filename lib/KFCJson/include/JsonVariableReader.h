/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include "JsonString.h"
#include "JsonBaseReader.h"

namespace JsonVariableReader {

    class Element;

    class Result {
    public:
        Result() {
        }
        virtual ~Result() {
        }

        virtual Result* create() = 0;

        //virtual Result *create() {
        //    // destroy results
        //    *this = Result();
        //    return nullptr;
        //    // save results
        //    return new Result(std::move(*this));
        //}
    };

    class Reader;

    class Element {
    public:
        typedef std::function<bool(Result & result, Reader & reader)> AssignCallback;

        Element(const JsonString& path, AssignCallback callback = nullptr);
        JsonString& getPath();
        bool callback(Result& result, Reader& reader);

    protected:
        JsonString _path;
        AssignCallback _callback;
    };

    class ElementGroup {
    public:
        typedef Element* ElementPtr;
        typedef std::vector<ElementPtr> ElementsVector;
        typedef Result* ResultPtr;
        typedef std::vector<ResultPtr> ResultsVector;
        typedef std::vector<ElementGroup> Vector;

        ElementGroup(const JsonString& path);
        ~ElementGroup();

        Element *add(const JsonString &path, Element::AssignCallback callback = nullptr);
        Element *add(Element *var);
        bool isPath(const String &path);
        Element *findPath(const String &path);
        void addResults();
        Result *getResultObject();


        template <class T>
        std::vector<T *>& getResults() {
            return reinterpret_cast<std::vector<T *> &>(_results);
        }

        template <class T>
        T *getResult() {
            if (_results.size()) {
                return reinterpret_cast<T *>(_results.front());
            }
            return nullptr;
        }

        template<class T>
        void setResult() {
            if (_result) {
                delete _result;
            }
            _result = new T();
        }

    private:
        JsonString _path;
        ElementsVector _elements;
        ResultsVector _results;
        Result* _result;
    };

    class Reader : public JsonBaseReader {
    public:
        Reader();
        ~Reader();

        ElementGroup::Vector* getElementGroups();

        virtual bool beginObject(bool isArray);
        virtual bool endObject();
        virtual bool processElement();
        virtual bool recoverableError(JsonErrorEnum_t errorType);

    private:
        ElementGroup::Vector* _elementGroups;
        ElementGroup* _current;
        int _level;
        bool _skip;
    };

};
