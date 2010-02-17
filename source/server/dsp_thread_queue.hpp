//  dsp thread queue
//  Copyright (C) 2007, 2008, 2009, 2010 Tim Blechmann
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; see the file COPYING.  If not, write to
//  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
//  Boston, MA 02111-1307, USA.

#ifndef SERVER_DSP_THREAD_QUEUE_HPP
#define SERVER_DSP_THREAD_QUEUE_HPP

#include <vector>
#include <memory>
#include <algorithm>

#include <boost/atomic.hpp>
#include <boost/cstdint.hpp>
#include <boost/thread.hpp>

#include <boost/lockfree/fifo.hpp>

#include "nova-tt/semaphore.hpp"
#include "utilities/branch_hints.hpp"

namespace nova
{

template <typename runnable, typename Alloc>
class dsp_queue_interpreter;

template <typename runnable, typename Alloc>
class dsp_threads;

/*
concept runnable
{
    runnable(const & runnable);

    operator()(uint threadindex);
};

*/

/** item of a dsp thread queue
 *
 * \tparam Alloc allocator for successor list and operator new/delete
 *
 * \todo operator new doesn't support stateful allocators
 */
template <typename runnable,
          typename Alloc = std::allocator<void*> >
class dsp_thread_queue_item:
    private Alloc
{
    typedef boost::uint_fast16_t activation_limit_t;
    typedef nova::dsp_queue_interpreter<runnable, Alloc> dsp_queue_interpreter;

    typedef typename Alloc::template rebind<dsp_thread_queue_item>::other new_allocator;

public:
    void* operator new(std::size_t size)
    {
        return new_allocator().allocate(size);
    }

    inline void operator delete(void * p)
    {
        new_allocator().deallocate((dsp_thread_queue_item*)p, sizeof(dsp_thread_queue_item));
    }

    typedef std::vector<dsp_thread_queue_item*, Alloc> successor_list;

    dsp_thread_queue_item(runnable const & job, successor_list const & successors,
                          activation_limit_t activation_limit):
        activation_count(0), job(job), successors(successors), activation_limit(activation_limit)
    {}

    dsp_thread_queue_item * run(dsp_queue_interpreter & interpreter, boost::uint8_t thread_index)
    {
        assert(activation_count == 0);

        job(thread_index);

        dsp_thread_queue_item * next = update_dependencies(interpreter);
        reset_activation_count();
        return next;
    }

    /** called from the run method or once, when dsp queue is initialized */
    void reset_activation_count(void)
    {
        assert(activation_count == 0);
        activation_count.store(activation_limit, boost::memory_order_release);
    }

    runnable const & get_job(void) const
    {
        return job;
    }

private:
    /** \brief update all successors and possibly mark them as runnable */
    dsp_thread_queue_item * update_dependencies(dsp_queue_interpreter & interpreter)
    {
        std::auto_ptr<dsp_thread_queue_item> ptr;
        std::size_t i = 0;
        for (;;)
        {
            if (i == successors.size())
                return NULL;

            successors[i++]->dec_ref_count(interpreter, ptr);
            if (ptr.get())
                break; // no need to update the next item to run
        }

        while (i != successors.size())
            successors[i++]->dec_ref_count(interpreter);
        return ptr.release();
    }

    /* @{ */
    /** \brief decrement reference count and possibly mark as runnable */
    inline void dec_ref_count(dsp_queue_interpreter & interpreter, std::auto_ptr<dsp_thread_queue_item> & ptr)
    {
        activation_limit_t current = activation_count--;
        assert(current > 0);

        if (current == 1)
        {
            if (ptr.get() == NULL)
                ptr.reset(this);
            else
                interpreter.mark_as_runnable(this);
        }
    }

    inline void dec_ref_count(dsp_queue_interpreter & interpreter)
    {
        activation_limit_t current = activation_count--;
        assert(current > 0);

        if (current == 1)
            interpreter.mark_as_runnable(this);
    }
    /* @} */

    boost::atomic<activation_limit_t> activation_count; /**< current activation count */

    runnable job;
    successor_list successors;                                 /**< list of successing nodes */
    const activation_limit_t activation_limit;                 /**< number of precedessors */
};

template <typename runnable, typename Alloc = std::allocator<void*> >
class dsp_thread_queue
{
    typedef boost::uint_fast16_t node_count_t;

    typedef nova::dsp_thread_queue_item<runnable, Alloc> dsp_thread_queue_item;

public:
    dsp_thread_queue(void):
        total_node_count(0)
    {}

    ~dsp_thread_queue(void)
    {
        for (std::size_t i = 0; i != queue_items.size(); ++i)
            delete queue_items[i];
    }

    void add_initially_runnable(dsp_thread_queue_item * item)
    {
        initially_runnable_items.push_back(item);
    }

    /** takes ownership */
    void add_queue_item(dsp_thread_queue_item * item)
    {
        queue_items.push_back(item);
        ++total_node_count;

        assert (total_node_count == queue_items.size());
    }

    void reset_activation_counts(void)
    {
        assert(total_node_count == queue_items.size());

        for (node_count_t i = 0; i != total_node_count; ++i)
            queue_items[i]->reset_activation_count();
    }

    node_count_t get_total_node_count(void) const
    {
        return total_node_count;
    }

private:
    node_count_t total_node_count;      /* total number of nodes */

    typename dsp_thread_queue_item::successor_list initially_runnable_items; /* nodes without precedessor */
    std::vector<dsp_thread_queue_item*> queue_items;                         /* all nodes */

    friend class dsp_queue_interpreter<runnable, Alloc>;
};

template <typename runnable,
          typename Alloc = std::allocator<void*> >
class dsp_queue_interpreter
{
protected:
    typedef nova::dsp_thread_queue<runnable, Alloc> dsp_thread_queue;
    typedef nova::dsp_thread_queue_item<runnable, Alloc> dsp_thread_queue_item;
    typedef typename dsp_thread_queue_item::successor_list successor_list;
    typedef std::size_t size_t;

public:
    typedef boost::uint_fast8_t thread_count_t;
    typedef boost::uint_fast16_t node_count_t;

    typedef std::auto_ptr<dsp_thread_queue> dsp_thread_queue_ptr;

    dsp_queue_interpreter(thread_count_t tc):
        fifo(1024), node_count(0)
    {
        set_thread_count(tc);
    }

    /** prepares queue and queue interpreter for dsp tick
     *
     *  \return true, if dsp queue is valid
     *          false, if no dsp queue is available or queue is empty
     */
    bool init_tick(void)
    {
        if (unlikely((queue.get() == NULL) or                /* no queue */
                     (queue->get_total_node_count() == 0)    /* no nodes */
                    ))
            return false;

        /* reset node count */
        assert(node_count == 0);
        assert(fifo.empty());
        node_count.store(queue->get_total_node_count(), boost::memory_order_release);

        successor_list const & initially_runnable_items = queue->initially_runnable_items;
        for (size_t i = 0; i != initially_runnable_items.size(); ++i)
            mark_as_runnable(initially_runnable_items[i]);

        return true;
    }

    dsp_thread_queue_ptr release_queue(void)
    {
        dsp_thread_queue_ptr ret(queue.release());
        return ret;
    }

    dsp_thread_queue_ptr reset_queue(dsp_thread_queue_ptr & new_queue)
    {
        dsp_thread_queue_ptr ret(queue.release());
        queue = new_queue;
        if (queue.get() == 0)
            return ret;

        queue->reset_activation_counts();

        thread_count_t thread_number =
            std::min(thread_count_t(std::min(total_node_count(),
                                             node_count_t(std::numeric_limits<thread_count_t>::max()))),
                     thread_count);

        used_helper_threads = thread_number - 1; /* this thread is not waked up */
        return ret;
    }

    node_count_t total_node_count(void) const
    {
        return queue->get_total_node_count();
    }

    void set_thread_count(thread_count_t i)
    {
        assert (i < std::numeric_limits<thread_count_t>::max());
        i = std::max(thread_count_t(1u), i);
        thread_count = i;
    }

    thread_count_t get_thread_count(void) const
    {
        return thread_count;
    }

    thread_count_t get_used_helper_threads(void) const
    {
        return used_helper_threads;
    }

    void tick(thread_count_t thread_index)
    {
        run_item(thread_index);
    }


private:
    void run_item(thread_count_t index)
    {
        for (;;)
        {
            if (node_count.load(boost::memory_order_acquire))
            {
                /* we still have some nodes to process */
                int state = run_next_item(index);

                if (state == no_remaining_items)
                    return;
            }
            else
                return;
        }
    }

public:
    void tick_master(void)
    {
        run_item_master();
    }

private:
    void run_item_master(void)
    {
        run_item(0);
        wait_for_end();
        assert(fifo.empty());
    }

    void wait_for_end(void)
    {
        while (node_count.load(boost::memory_order_acquire) != 0)
        {} // busy-wait for helper threads to finish
    }

    int run_next_item(thread_count_t index)
    {
        dsp_thread_queue_item * item;
        bool success = fifo.dequeue(&item);

        if (!success)
            return fifo_empty;

        node_count_t consumed = 0;

        do
        {
            item = item->run(*this, index);
            consumed += 1;
        } while (item != NULL);

        node_count_t remaining = node_count.fetch_sub(consumed, boost::memory_order_release);

        assert (remaining >= consumed);

        if (remaining == consumed)
            return no_remaining_items;
        else
            return remaining_items;
    }

    friend class nova::dsp_threads<runnable, Alloc>;

    void mark_as_runnable(dsp_thread_queue_item * item)
    {
        fifo.enqueue(item);
    }

    friend class nova::dsp_thread_queue_item<runnable, Alloc>;

private:
    enum {
        no_remaining_items,
        fifo_empty,
        remaining_items
    };

    dsp_thread_queue_ptr queue;

    thread_count_t thread_count;        /* number of dsp threads to be used by this queue */
    thread_count_t used_helper_threads; /* number of helper threads, which are actually used */

    boost::lockfree::fifo<dsp_thread_queue_item*> fifo;
    boost::atomic<node_count_t> node_count; /* number of nodes, that need to be processed during this tick */
};

} /* namespace nova */

#endif /* SERVER_DSP_THREAD_QUEUE_HPP */
