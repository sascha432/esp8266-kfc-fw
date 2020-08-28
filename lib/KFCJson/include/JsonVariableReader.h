/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include "JsonString.h"
#include "JsonBaseReader.h"

#include <debug_helper_enable_mem.h>
namespace JsonVariableReader {

    class Element;

    class Result {
    public:
        Result() {
        }
        virtual ~Result() {
        }

        Result(const Result &) = delete;
        Result(Result &&) = default;
        Result &operator=(Result &&) = default;

        virtual bool empty() const = 0;

        virtual Result *create() = 0;

        //virtual Result *create() {
        //    // destroy results
        //    *this = Result();
        //    return nullptr;
        //    // save results
        //    return __LDBG_new(Result, std::move(*this));
        //}
    };

    class Reader;

    class Element {
    public:
        typedef std::function<bool(Result &result, Reader &reader)> AssignCallback;

        Element(const Element &) = delete;

        Element(const JsonString &path, AssignCallback callback = nullptr);
        JsonString &getPath();
        bool callback(Result &result, Reader &reader);

    protected:
        JsonString _path;
        AssignCallback _callback;
    };

    class ElementGroup {
    public:
        typedef Element *ElementPtr;
        typedef std::vector<ElementPtr> ElementsVector;
        typedef Result *ResultPtr;
        typedef std::vector<ResultPtr> ResultsVector;
        typedef std::vector<ElementGroup> Vector;

        ElementGroup(const JsonString &path);
        ~ElementGroup();

        ElementGroup(const ElementGroup &) = delete;
        ElementGroup(ElementGroup &&) = default;
        ElementGroup &operator=(ElementGroup &&) = default;

        ElementPtr add(const JsonString &path, Element::AssignCallback callback = nullptr);
        ElementPtr add(Element *var);
        bool isPath(const String &path);
        ElementPtr findPath(const String &path);

        // adds a new result object stored using _results::create
        void flushResult();

        // retrieves last result added
        ResultPtr getLastResult();

        // retrieve stored results
        template <class T>
        std::vector<T *> &getResults() {
            _results.erase(std::remove_if(_results.begin(), _results.end(), [](Result *result) {
                if (result->empty()) {
                    __LDBG_delete(result);
                    return true;
                }
                return false;
            }), _results.end()); // remove invalid results
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
            return reinterpret_cast<std::vector<T *> &>(_results);
#pragma GCC diagnostic pop
        }

        // set result class type
        template <class T>
        void initResultType() {
            _results.emplace_back(__LDBG_new(T));
        }

    private:
        JsonString _path;
        ElementsVector _elements;
        ResultsVector _results;
    };

    class Reader : public JsonBaseReader {
    public:
        Reader();
        ~Reader();

        ElementGroup::Vector *getElementGroups();

        virtual bool beginObject(bool isArray);
        virtual bool endObject();
        virtual bool processElement();
        virtual bool recoverableError(JsonErrorEnum_t errorType);

    private:
        ElementGroup::Vector *_elementGroups;
        ElementGroup *_current;
        int _level;
        bool _skip;
    };

};

#include <debug_helper_disable_mem.h>
