#include "tasking.hpp"

#include <algorithm>

namespace {
	constexpr size_t get_target_executors_count() noexcept {
		// TODO: implement strategy
		return 3;
	}

	ptrdiff_t round_wrap_index(ptrdiff_t index, size_t count) noexcept {
		// works for indexes (boundary +- count), out of bound if greater dynamic range
		return index
			+ (ptrdiff_t)(count & (-(index < 0)))
			- (ptrdiff_t)(count & (-(index >= (ptrdiff_t)(count))));
	}
}


// task_pool implementation section:
//

task_pool::~task_pool() {
	for (ptrdiff_t i = 1; i < this->executors.size(); ++i) {
		if (this->threads[i].joinable()) {
			this->executors[i]->shutdown();
			this->threads[i].join();
		}
	}
}

task_pool::task_pool(): threads(get_target_executors_count()), executors(get_target_executors_count()) {
	// this->threads[0] remains default-initialized, placeholder for this_thread to cope 
	// with index shifting relative to this->executors.
	// this->executors[0] is reserved for current thread, does not execute concurrently.

	for (ptrdiff_t i = 0; i < this->executors.size(); ++i) {
		this->executors[i] = std::make_unique<task_executor>(*this);
		this->executor_index_map.insert({ this->executors[i].get(), i });
	}
}

bool task_pool::schedule(task_executor& executor) {
	std::unique_lock lock(this->scheduling_mx, std::try_to_lock);
	if (lock.owns_lock()) {
		struct update_epoch_on_return {
			std::atomic<ptrdiff_t>& counter;
			bool epoch_ended = false;

			~update_epoch_on_return() {
				counter.fetch_add(1);	// TODO: relax mem order? load+store?
				counter.fetch_xor(-epoch_ended);
				counter.notify_all();
			};

			update_epoch_on_return(std::atomic<ptrdiff_t>& counter) noexcept : counter(counter) {}

			void queue_exhausted() noexcept {
				epoch_ended = true;
			}
		} epoch_updater(this->schedule_epoch_counter);

		std::list<managed_task_item> schedule_queue_local_buffer;
		{
			std::lock_guard lock(this->schedule_queue_mx);
			schedule_queue_local_buffer.swap(this->global_schedule_queue);
		}

		// happens-before with some preceding write via lock acquire
		if (schedule_queue_local_buffer.empty()) {
			// handle unseccuessful scheduling - return from function
			bool result = executor.steal_cycle([this]() -> bool {
					std::lock_guard lock(this->schedule_queue_mx);
					return this->global_schedule_queue.empty();
				});

			if (!result) {
				// wait on all executors state
				{
					std::vector<task_executor*> busy_executors;
					for (auto& item : this->executors) {
						if (item->get_state() == task_executor::state::busy &&
								item.get() != &executor) {
							busy_executors.push_back(item.get());
						}
					}

					ptrdiff_t old = 0;
					this->state_subscription_counter = old; // reset counter 
					do {
						decltype(busy_executors)::iterator it = busy_executors.begin();
						while (it != busy_executors.end()) {
							if ((*it)->get_state() > task_executor::state::busy) {
								it = busy_executors.erase(it);
							} else {
								++it;
							}
						}

						if (busy_executors.empty()) {
							break;
						}

						old = (this->state_subscription_counter -= old);
						this->state_subscription_counter.wait(0);

					// happens-before with some preceding write via acquire in 
					// state_subscription_counter
					} while (this->global_schedule_queue.empty());	
				}

				std::lock_guard lock(this->schedule_queue_mx);
				if (this->global_schedule_queue.empty()) {
					// scheduling unsuccessful: ran out of tasks, nothing to schedule
					epoch_updater.queue_exhausted();
					return false;
				}

				schedule_queue_local_buffer.swap(this->global_schedule_queue);
			} // global queue is not empty otherwise
			else {
				// else branch to avoid consequtive excessive locking =(
				std::lock_guard lock(this->schedule_queue_mx);
				schedule_queue_local_buffer.swap(this->global_schedule_queue);
			}
		}
		std::vector<size_t> enqueued_counts(this->executors.size());
			
		for (ptrdiff_t i = 0; i < this->executors.size(); ++i) {
			enqueued_counts[i] = this->executors[i]->enqueued_count();
		}

		size_t total_task_count =
			std::reduce(enqueued_counts.cbegin(), enqueued_counts.cend(), schedule_queue_local_buffer.size());

		std::vector<size_t> enqueue_amount(this->executors.size(), 
			(total_task_count + (this->executors.size() - 1)) / this->executors.size());
		for (ptrdiff_t i = 0; i < this->executors.size(); ++i) {
			// take care of unsigned underflow
			using signed_amount_t = std::make_signed_t<decltype(enqueue_amount)::value_type>;
			enqueue_amount[i] = relu(((signed_amount_t)enqueue_amount[i]) - enqueued_counts[i] + 
				(enqueued_counts[i] == 0));
		}

		ptrdiff_t executor_index = this->executor_index_map[&executor];

		bool schedule_next_batch = true;
		constexpr size_t unscheduled_remainder_threshold = 0;
		do {
			size_t queue_remaining_size = schedule_queue_local_buffer.size();
			auto current_task = schedule_queue_local_buffer.begin();
			for (ptrdiff_t i = 0; i < enqueue_amount.size(); ++i) {
				size_t tasks_to_enqueue = std::min(enqueue_amount[executor_index], queue_remaining_size);
				auto end_iterator = std::next(current_task, tasks_to_enqueue);

				// this op potentially syncs with other executors that may try stealing
				std::list<managed_task_item> enqueue_buffer(std::make_move_iterator(current_task),
					std::make_move_iterator(end_iterator));
				this->executors[executor_index]->enqueue(std::move(enqueue_buffer));
				this->executors[executor_index]->wake_up(); // check if necessary here or should be implemented another way
				current_task = end_iterator;

				queue_remaining_size -= tasks_to_enqueue;
				if (queue_remaining_size == 0) {
					break;
				}

				executor_index &= -(++executor_index < executors.size()); // round-up bound limit
			}

			schedule_queue_local_buffer.clear();
			{
				std::lock_guard lock(this->schedule_queue_mx);
				schedule_next_batch = (this->global_schedule_queue.size() > unscheduled_remainder_threshold);
				if (schedule_next_batch) {
					schedule_queue_local_buffer.swap(this->global_schedule_queue);
				}
			}
			std::fill(enqueue_amount.begin(), enqueue_amount.end(),
				(schedule_queue_local_buffer.size() + (this->executors.size() - 1)) / this->executors.size());
		} while (schedule_next_batch);

		// scheduling is successful, schedule next scheduling task
		this->enqueue_scheduling_task(*(this->executors[executor_index]));
	} else {
		// debug me, several scheduling tasks attempted to execute simultaneously.
	}

	return lock.owns_lock();
}

void task_pool::execute_flow(bool async) {
	bool valid = true;
	valid &= !this->executors.empty();
	valid &= !(async & (this->executors.size() < 2));
	if (!valid) {
		// TODO: error handling
	}

	// well, preconditions...
	valid &= this->executors[0] != nullptr;
	if (!valid) {
		// TODO: error handling
	}

	bool initialized = std::any_of(this->executors.begin(), this->executors.end(),
		[](const auto& executor_ptr) -> bool {
			return (executor_ptr->get_state() != task_executor::state::init);
		});

	if (initialized) {
		// skip this->executors[0], reserved for calling thread
		bool need_scheduling = std::all_of(std::next(this->executors.begin()), this->executors.end(),
			[](const auto& executor_ptr) -> bool {
				task_executor::state current_state = executor_ptr->get_state();
				return (current_state == task_executor::state::halted) |
					// for the purpose of makind decision on scheduling necessity 
					// halted, pending_shutdown and shutdown are equivalent
					(current_state == task_executor::state::pending_shutdown) |
					(current_state == task_executor::state::shutdown);
			});
		if (need_scheduling) {
			this->schedule_epoch_counter.store(0);
			this->schedule(*(this->executors[0]));
		} // otherwise running executors will eventually schedule tasks from global queue

		// TODO: should we restart executors explicitly?
		// for (ptrdiff_t i = 1; i < this->executors.size(); ++i) {
		// 	task_executor::state current_state = this->executors[i]->get_state();
		// 	if (current_state == task_executor::state::pending_shutdown) {
		// 		if (this->threads[i].joinable()) {
		// 			// restart thread and executor
		// 			this->threads[i].join();
		// 			this->threads[i] = std::thread(&task_executor::start_asynchronous_execution, this->executors[i].get());
		// 		}
		// 		else {
		// 			// TODO: invariant violation, debug me
		// 		}
		// 	}
		// }
	} else {
		// we can always keep executor[0] for calling thread.
		this->schedule(*(this->executors[0]));

		for (ptrdiff_t i = 1; i < this->executors.size(); ++i) {
			// TODO: check std::thread join/move preconditions
			this->threads[i] = std::thread(&task_executor::start_asynchronous_execution, this->executors[i].get());
		}
	}

	if (!async) {
		// join pool
		this->executors[0]->start_synchronous_execution();
	}
}

void task_pool::enqueue_scheduling_task(task_executor& executor) {
	class schedule_task {
	public:
		void execute(task_executor& executor) {
			executor.handle_scheduling();
		}
	};

	executor.enqueue(this->generator.create_task(schedule_task()));
	// TODO: generator.create_task is potentially concurrent with generator.create_task 
	// in public add tasks; the first executed by some executor during scheduling [under 
	// mutex], the latter executed by external code owning task_pool instance.
	//
}

std::vector<task_pool::task_executor*> task_pool::get_stealing_target_sequence(task_executor& initiator) {
	// TODO: check collection boundaries handling (the rest of items should be processed 
	// subsequently in any scenario)

	std::vector<task_executor*> result(this->executors.size() - 1, nullptr); // omit initiator self 

	ptrdiff_t initiator_index = this->executor_index_map[&initiator];
	for (ptrdiff_t i = 0; i < result.size(); ++i) {
		ptrdiff_t offset = ((i + 1) >> 1) ^ (-((i + 1) & 0x01));
		result[i] = this->executors[round_wrap_index(initiator_index + offset, this->executors.size())].get();
	}

	return result;
}

void task_pool::enqueue_scheduler(std::list<task_pool::managed_task_item>&& tasks) noexcept {
	std::lock_guard lock(this->schedule_queue_mx);
	global_schedule_queue.splice(global_schedule_queue.cend(), std::move(tasks));
}

void task_pool::enqueue_scheduler(std::list<task_pool::managed_task_item>& tasks) noexcept {
	std::lock_guard lock(this->schedule_queue_mx);
	global_schedule_queue.splice(global_schedule_queue.cend(), tasks);
}

void task_pool::notify_executor_state_change() noexcept {
	++(this->state_subscription_counter);
	this->state_subscription_counter.notify_all();
}


// task_executor implementation section:
//

task_pool::task_executor::task_executor(task_pool& parent_pool) : 
		parent_pool(parent_pool), current_state(state::init) {}

template <typename F>
void task_pool::task_executor::execution_cycle(F&& on_halt_handler) {
	{
		state expected_init = state::init;
		state expected_shutdown = state::shutdown;
		bool valid =
			this->current_state.compare_exchange_strong(expected_init, state::busy) |
			this->current_state.compare_exchange_strong(expected_shutdown, state::busy);
		if (!valid) {
			// TODO: handle initialization error: likely concurrent initialization attempt
			// 
			// use expected_shutdown value for error analysis
			//
		}
	}

	bool should_try_stealing = false;
	{
		// need to sync with potentially concurrent steal attempt
		std::lock_guard lock(this->execution_queue_mx);
		should_try_stealing = this->local_execution_queue.empty();
	}

	while (this->current_state.load(std::memory_order_acquire) != state::pending_shutdown) {
		if (should_try_stealing) {
			auto pred = [this, &should_try_stealing]() -> bool {
				std::lock_guard lock(this->execution_queue_mx);
				return (should_try_stealing = this->local_execution_queue.empty());
			};

			if (!this->steal_cycle(pred)) {
				state expected_busy = state::busy;
				if (this->current_state.compare_exchange_strong(expected_busy, state::halted)) {
					parent_pool.notify_executor_state_change();
					on_halt_handler();
				} else {
					// concurrent state change, the only valid one is shutdown request
				}
			}
		} else {
			auto acquire_task = [this, &should_try_stealing]() {
				std::lock_guard lock(this->execution_queue_mx);
				managed_task_item current_task = std::move(this->local_execution_queue.front());
				this->local_execution_queue.pop_front();
				should_try_stealing = this->local_execution_queue.empty();

				this->weak_execution_queue_size.store(this->local_execution_queue.size(),
					std::memory_order_relaxed);

				return current_task;
			};

			this->execute_task(acquire_task());
		}
	}

	this->current_state = state::shutdown; // TODO: mem order?
	parent_pool.notify_executor_state_change();
}

template <typename F>
bool task_pool::task_executor::steal_cycle(F&& pred) {
	// copy-paste from execution cycle. Keep this predicated version until it is clear
	// how to implement this interface properly

	std::vector<task_executor*> steal_targets = this->parent_pool.get_stealing_target_sequence(*this);
	decltype(steal_targets)::iterator it = steal_targets.begin();
	// do {
	while (pred()) {
		if (it == steal_targets.end()) {
			return false;
		}

		if (this->execute_task((*it)->try_steal_task())) {
			++it;
		} else {
			it = steal_targets.erase(it);
		}

		// round wrap
		if (it == steal_targets.end()) {
			it = steal_targets.begin();
		}
	}
	// } while (pred());

	return true;
}

task_pool::managed_task_item task_pool::task_executor::try_steal_task() noexcept {
	managed_task_item stolen_task;

	if (this->execution_queue_mx.try_lock()) {
		std::lock_guard lock(this->execution_queue_mx, std::adopt_lock);

		state self_state = this->current_state.load(std::memory_order_relaxed);
		bool proceed_stealing =
			// do not pop the last element in the queue to meet precondition for pop_front
			// in the execution cycle code
			(this->local_execution_queue.size() > 1);

		proceed_stealing |=
			// if the executor is not active, the last task would not be executed otherwise, 
			// that would result in tasking locks or scheduling failures
			((self_state == state::init) |
				(self_state == state::shutdown)) &
			(this->local_execution_queue.size() > 0);

		if (proceed_stealing) {
			// get the task from the end of the queue: the latest task that could be scheduled
			// to the executor that initiates task steal if the scheduler was a bit more lucky
			stolen_task = std::move(local_execution_queue.back());
			local_execution_queue.pop_back();
			// this->execution_queue_mem_sync.publish();
			this->weak_execution_queue_size.store(this->local_execution_queue.size(),
				std::memory_order_relaxed);
		}
	} else {
		// behave as stealing attempt was successfull, so that this neighbour is 
		// not removed from stealing sequence collection. Maybe the next time lucky.

		class dummy_task {
		public:
			void execute(task_executor& executor) {
				return;
			}
		};

		stolen_task = this->create_task(dummy_task());
		// TODO: task_generator::create_task here is potentially concurrent with 
		// task_generator::create_task in put_task; the first is performed by 
		// neighbour executor on *this executor context, the latter is performed 
		// by *this executor on task finish (inside task_base derived classes)
		//
	}
	return stolen_task;
}

void task_pool::task_executor::start_synchronous_execution() {
	auto on_halt_handler = [this]() -> void {
		// shutdown current executor so that execution cycle exits. The flow then 
		// returns to the caller.

		// flow execution always ends with failed scheduling attempt. If scheduling fails, 
		// then all executors are halted and no tasks available in schediling queue.
		// 
		// after scheduling is completed, there're either available tasks in execution queue, 
		// either other executors are busy, either the end of flow reached.
		//

		// seq_cst is needed here
		ptrdiff_t epoch = parent_pool.schedule_epoch_counter.load();
		state expected_halted = this->current_state.load();
		// no need to wait for scheduling if executor is waken up since state was set to halted
		if ((expected_halted == state::halted) & (epoch >= 0)) {
			// there's a chance that after scheduling is done we get some tasks, 
			// or some neighbour gets tasks and we could try stealing
			parent_pool.schedule_epoch_counter.wait(epoch);
			epoch = parent_pool.schedule_epoch_counter.load();
		}

		if (epoch < 0) {
			// end of flow reached
			this->current_state.store(state::pending_shutdown);
		}
	};

	this->execution_cycle(on_halt_handler);
}

void task_pool::task_executor::start_asynchronous_execution() {
	auto on_halt_handler = [this]() -> void {
		this->current_state.wait(state::halted);
	};

	this->execution_cycle(on_halt_handler);
}

void task_pool::task_executor::wake_up() {
	state expected_halted = state::halted;
	bool change_applied = this->current_state.compare_exchange_strong(expected_halted, state::busy);
	if (change_applied) {
		this->current_state.notify_all();
	}
}

void task_pool::task_executor::shutdown() {
	// precondition: state is not init
	// 
	// potentially races with execution cycle. 
	// 
	// Mitigation:
	//	1. compare_excchange_strong from init to shutdown
	//	2. if (1.) successful, there may be race, but it's caller to blame himself.
	//		behavior is well-defined, but maybe throw is reasonable
	//	3. if (2.) is not successfull, exchange to pending_shutdown
	//	4. if result of (3.) is shutdown, store shutdown.
	//		behavior is well-defined, but race is possible, winner unspecified
	// 

	state old = state::init;
	bool valid = !this->current_state.compare_exchange_strong(old, state::shutdown);
	if (!valid) {
		// throw?
	} else {
		old = this->current_state.exchange(state::pending_shutdown);
		if (old == state::shutdown) {
			this->current_state.store(state::shutdown);
		}
		this->current_state.notify_all();
	}
}

bool task_pool::task_executor::execute_task(managed_task_item&& task_item) {
	bool result = false;
	if (task_item) {
		try {
			task_item.task->execute(*this);
			result = true;
		} catch (const std::exception& e) {

		} catch (...) {

		}

		// Move members should be noexcept here. If exception is thrown for push_back, 
		// let it propogate up to the stack root (likely we ran out of memory).
		// Is it reasonable to trigger std::terminate in that case?
		this->retired_tasks.push_back(std::move(task_item.descriptor));

		if (!this->local_schedule_queue.empty()) {
			this->parent_pool.enqueue_scheduler(this->local_schedule_queue);
		}
	} else {
		// debug me, illegal concurrent execution attempt
	}
	return result;
}

void task_pool::task_executor::enqueue(managed_task_item&& task_item) {
	std::lock_guard lock(this->execution_queue_mx);
	this->local_execution_queue.push_back(std::move(task_item));
}

void task_pool::task_executor::enqueue(std::list<managed_task_item>&& tasks) noexcept {
	// noexcept in declaration because std::list::splice "Throws: Nothing." per the standard
	std::lock_guard lock(this->execution_queue_mx);
	this->local_execution_queue.splice(this->local_execution_queue.end(), std::move(tasks));
	this->weak_execution_queue_size.store(this->local_execution_queue.size(),
		std::memory_order_relaxed);
}

void task_pool::task_executor::handle_scheduling() {
	// supposed to happen either in the context of normal flow, either in the context of
	// task stealing
	bool scheduling_successful = this->parent_pool.schedule(*this);
}

size_t task_pool::task_executor::enqueued_count() const noexcept {
	return this->weak_execution_queue_size.load(std::memory_order_relaxed);
}

task_pool::task_executor::state task_pool::task_executor::get_state() const {
	return this->current_state;
}
