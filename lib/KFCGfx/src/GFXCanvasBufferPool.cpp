/**
* Author: sascha_lammers@gmx.de
*/


#include <Arduino_compat.h>
#include "GFXCanvasConfig.h"
#include "GFXCanvasBufferPool.h"
#include "GFXCanvasByteBuffer.h"

#if 0

#pragma GCC push_options
#if DEBUG_GFXCANVAS
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#pragma GCC optimize ("O3")
#endif

using namespace GFXCanvas;

BufferPool *BufferPool::_instance = nullptr;

// void BufferPool::moveTempTo(ByteBuffer &buffer)
// {
//     auto iterator = add(&buffer);
//     __LDBG_printf("iterator ptr=%p", &(*iterator));
//     auto &header = *iterator;
//     buffer._capacity = 0;
//     buffer._size = header.dataSize();
//     buffer._data = header.data();
//     _temp.setLength(0);
//     _temp.resize(kTempBufferInitSize);
//     // #if _MSC_VER || DEBUG_GFXCANVAS
//     //     std::fill_n(_temp.begin(), _temp.size(), 0xcc);
//     // #endif
//     // std::fill(_temp.begin(), _temp.end(), 0);
// }

BufferPool::BufferIterator BufferPool::find(BufferTypePtr id)
{
    __LDBG_printf("ptr=%p", id);
    for (auto &buffer : _list) {
        __LDBG_printf("buffer_ptr=%p begin=%p size=%u", &buffer, buffer.begin(), buffer.size());
        for(auto iterator = BufferIterator(buffer); iterator; ++iterator) {
            __LDBG_printf("iter_ptr=%p iter=%u", *iterator, (bool)iterator);
            if (*iterator == id) {
                return iterator;
            }
        }
    }
    __LDBG_printf("not found");
    return BufferIterator();
}

bool BufferPool::remove(BufferTypePtr id)
{
    for (auto &buffer : _list) {
        for (auto iterator = BufferIterator(buffer); iterator; ++iterator) {
            auto &header = *iterator;
            if (header == id) {
                // remove header
                buffer.remove(header.begin() - buffer.begin(), header.size());
                if (buffer.length() == 0) {
                    // remove empty buffer from pool
                    _list.erase(std::find(_list.begin(), _list.end(), buffer), _list.end());
                }
                return true;
            }
        }
    }
    return false;
}

BufferPool::BufferIterator BufferPool::add(BufferTypePtr id)
{
    __LDBG_printf("ptr=%p", id);
    auto iterator = find(id);
    while (iterator) {
        // old buffer length greater or equal, reuse
        auto &header = *iterator;
        int diff = header.dataSize() - 0;//_temp.length();
        if (diff >= 0) {
            if (diff != 0) {
                // shrink data after header
                iterator->remove(header.data() - iterator->begin(), diff);
                // update size
                header._dataSize -= diff;
                __LDBG_printf("diff=%d", diff);
            }
        }
        else if (
            // old buffer is smaller, check if there is enough space left in the buffer
            header.end() - diff < iterator->begin() + iterator->size() &&
            // check if iterator points to the last item
            !(iterator + 1)
        ) {
            header._dataSize -= diff; // extend size
            __LDBG_printf("diff=%d", diff);
            iterator->setLength(iterator->length() - diff); // update buffer length as well
        }
        else {
            // no match, remove old buffer
            __LDBG_printf("remove=%p", id);
            remove(id);
            iterator = BufferIterator();
            break;
        }
        // copy new data
        // std::copy(_temp.begin(), _temp.end(), header.data());
        #if _MSC_VER || DEBUG_GFXCANVAS
            std::fill(iterator->end(), iterator->begin() + iterator->size(), 0xcc);
        #endif
        return iterator;
    }

    // find space, now we need a little extra for the header
    BufferHeader_t header(id, (size_t)0);//_temp.length());

    for (auto &buffer : _list) {
        if (buffer.available() >= header.size()) {
            iterator = BufferIterator(buffer, buffer.end());
            break;
        }
    }
    // no space left, add new buffer
    if (!iterator) {
        __LDBG_printf("new pool %u", _list.size());
        _list.emplace_back(kBufferPoolSize);
        iterator = BufferIterator(_list.back());
    }

    iterator->push_back(header);
    // iterator->write(_temp.begin(), _temp.length());
    #if _MSC_VER || DEBUG_GFXCANVAS
        std::fill(iterator->end(), iterator->begin() + iterator->size(), 0xcc);
    #endif
    return iterator;
}

BufferPool &BufferPool::getInstance()
{
    return *(_instance ? _instance : (_instance = new BufferPool()));
}

void GFXCanvas::BufferPool::destroyInstance()
{
    if (_instance) {
        delete _instance;
        _instance = nullptr;
    }
}

// Buffer &BufferPool::getTemp()
// {
//     return _temp;
// }

// Buffer &BufferPool::get()
// {
//     _temp.setLength(0);
//     _temp.resize(kTempBufferInitSize);
//     return _temp;
// }

void BufferPool::dump(Print &output)
{
    output.printf_P(PSTR("--- pool size=%u\n"), _list.size());

    uint32_t minId = ~0U;
    for (auto &buffer : _list) {
        for (auto iterator = BufferIterator(buffer); iterator; ++iterator) {
            auto id = (uint32_t)(*iterator).getId();
            if (id < minId) {
                minId = id;
            }
        }
    }

    size_t n = 0;
    size_t len = 0;
    size_t available = 0;
    size_t data = 0;
    for(auto &buffer: _list) {
        PrintString tmp;
        size_t lines = 0;
        for(auto iterator = BufferIterator(buffer); iterator; ++iterator) {
            auto &header = *iterator;
            tmp.printf_P(PSTR("%03d:%03u%c"), ((uint32_t)(header.getId()) - minId) / sizeof(uint32_t), header.dataSize(), (++lines % 10 == 0) ? '\n' : ' ');
            data += header.dataSize();
        }
        output.printf_P(PSTR("%u: used=%u size=%u lines=%u\n"), n, buffer.length(), buffer.size(), lines);
        output.println(tmp);
        n++;
        len += buffer.length();
        available += buffer.available();
    }
    output.printf_P(PSTR("data len=%u data=%u available=%u pool=%u\n"), len, data, available, _list.size());
}

#pragma GCC pop_options
#endif
