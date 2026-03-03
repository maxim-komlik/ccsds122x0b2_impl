#pragma once

// result for dwt transformation - bitmap set == subbands_t <templated by fp/integral type>
// result for segment assembly - vector of segments <templated by transformed integral type>
// result for file sink - filepath of the output file
// result for memory sink - pointer to a managed memory containing compressed data
// result for network sink - transmission finish timestamp and accumulative stream offset 
// 
// result for scheduling/management tasks - void
// 


// Compute graph is composed from the sequence of function signatures listed in the order 
// of compute flow. Each node specifies the payload function itself, set of input parameters 
// types and return type. 
// Each node defines a function/callable structure that takes the parameters of the current 
// node's input parameters' types, passes those parameters to the node function and gets the 
// result, then creates a task with the next node's callable, passing the current node's function 
// result as input parameter to the next node's callable.
// 
// The last node in the graph is a special case and does not create a task after the payload 
// function returns.
// 
// Since input parameters to every payload function of the next node is the result of the payload 
// function of the previous node, input parameters are handled in runtime. 
// Payload function should be passed as a compile time parameter, maybe wrapped in a lambda and 
// lambda type passed as a template parameter, instantiating the lambda when payload function 
// invocation is needed.
// 
// 
// task owns:
//		list of input parameters (std::tuple)
// task is templated with the following params:
//		parameter pack describing input parameter types
//		callable type with operator() to be called as a task payload; signature compatible 
//			to the parameters' types list
//		callable return type (may be deduced from callable type)
// task storage payload is a class template derived from concept class. Since callable 
// template type parameter is unique (based on lambda property), every task type is unique.
// Derived implementation is invoked via base's concept virtual member function.
// 
// 
// When composing graph flow wrap every payload call into a lambda, then pass the lambda to 
// the graph node factory function as a parameter. This would require duplication of input 
// parameters though
// 
// 
// should be like this:
// FlowGraph()
//		.then([](T1 pt1, T2 pt2){return action_001(pt1, pt2);})
//		.then([](T3 pt3, T4 pt4, T5 pt5){return action_002(pt3, pt4, pt5);})
//		.then([](T6 pt6){return action_003(pt6);})
//		.then([](T7 pt7, T8 pt8, T9 pt9){return action_004(pt7, pt8, pt9);})
// 
// this should result in a grapth that contains type template pack that consists of
// corresponding lambdas for action_001, action_002, action_003.
//

// consequtive:		current -> next				result = current() -> next(result)
// split:			current -> (next)*			result = current() -> item in result: next(item)
// fork:			current -> next1, next2...	result = current() -> next1(result), next2(result)
// terminal:		current						current()
// 
// FlowGraph().do([1]).then([2]).then([3]).then([4])
//	{start}		[1] --> [2] --> [3] --> [4]		{end}
//	consequtive<[1], consequtive<[2], consequtive<[3], terminal<[4]>>>>
// 
// FlowGraph().do([1]).split([2]).then([3]).split([4])
//	{start}		{1} +-> [2] --> [3] +-> [4]		{end}
//					|				+-> [4]
//					|				+-> [4]
//					|
//					+-> [2] --> [3] +-> [4]
//					|				+-> [4]
//					|				+-> [4]
//					|
//					+-> [2] --> [3] +-> [4]
//									+-> [4]
//									+-> [4]
//	split<[1], consequtive<[2], split<[3], terminal<[4]>>>>
// 
// template <typename Callable>
// auto fork(Callable c)&;
// 
// template <typename Callable>
// auto fork(Callable c)&& = delete;
// 
// auto graph_1 = FlowGraph().do([1]);
// auto graph_2 = graph_1.fork([2]).then([3]).split([4]);
// auto graph_3 = graph_1.fork([5]).then([6]).then([7]).split([4]).then([8]);
// 
// 
// auto graph_1 = FlowGraph().do([1]).fork();
// auto graph_2 = graph_1.do([2]).then([3]).split([4]);
// auto graph_3 = graph_1.do([5]).then([6]).split([4]).then([8]);
// auto final = graph_1.fuse(graph_2, graph_3);
// 
// std::tuple<decltype([1]), fork_token_t>
// std::tuple<decltype([1]), fork_token_t, 
//		decltype[2], decltype[3], std::array<std::tuple<decltype([4])>>>
// std::tuple<decltype([1]), fork_token_t, 
//		decltype[5], decltype[6], decltype[7], std::array<std::tuple<decltype([4]), decltype([8])>>>
// 
// FlowGraph::fuse ->
// std::tuple<decltype([1]), 
//		std::tuple<
//			std::tuple<decltype[2], decltype[3], std::array<decltype([4])>>, 
//			std::tuple<decltype[5], decltype[6], decltype[7], std::array<decltype([4])>>
//		>>
// 
// auto graph_1 = FlowGraph().do([1]).fork();
// auto graph_2 = graph_1.do([2]).then([3]).split([4]);
// auto graph_3 = graph_1.do([5]).then([6]).split([4]).then([8]);
// 
// auto graph_4 = graph_2.fork();
// auto graph_5 = graph_4.do([9]).then([10]);
// auto graph_6 = graph_4.do([11]);
// auto tip_merge = graph_4.fuse(graph_5, graph_6);
// auto final = graph_1.fuse(tip_merge, graph_3);
// 
//	{start}		{1} +-> {2} --> {3} +-> {4} +-> {9} --> {10}	{end}
//					|				|		+-> {11}
//					|				|
//					|				+-> {4} +-> {9} --> {10}
//					|				|		+-> {11}
//					|				|
//					|				+-> {4} +-> {9} --> {10}
//					|						+-> {11}
//					|				
//					+-> {5} --> {6} --> {7} +-> {4} --> {8}
//											+-> {4} --> {8}
//											+-> {4} --> {8}
// 
//	std::tuple<
//		std::tuple<
//			decltype({1}),
//			std::tuple<
//				std::tuple<
//					decltype({2}), 
//					decltype({3}), 
//					std::array<
//						std::tuple<
//							decltype({4}), 
//							std::tuple<
//								std::tuple<
//									decltype({9}), 
//									decltype({10})>, 
//								std::tuple<
//									decltype({11})>>>>>,
//				std::tuple<
//					decltype({5}), 
//					decltype({6}), 
//					decltype({7}), 
//					std::array<
//						std::tuple<
//							decltype({4}), 
//							decltype({8})>>>>>>
// 
//	fork_task<decltype({1}), 
//		consequtive_task<decltype({2}), 
//			split_task<decltype({3}), 
//				fork_task<decltype({4}), 
//					consequtive_task<decltype({9}), 
//						terminal_task<decltype({10})>>
//					terminal_task<decltype({11})>>>>, 
//		consequtive_task<decltype({5}), 
//			consequtive_task<decltype({6}), 
//				split_task<decltype({7}), 
//					consequtive_task<decltype({4}), 
//						terminal_task<decltype({8})>>>>>>
// 
// Chain generation step by step:
// 
// // Original snippet:
// // auto graph_1 = FlowGraph().do([1]).fork();
// // auto graph_2 = graph_1.do([2]).then([3]).split([4]);
// // auto graph_3 = graph_1.do([5]).then([6]).split([4]).then([8]);
// // 
// // auto graph_4 = graph_2.fork();
// // auto graph_5 = graph_4.do([9]).then([10]);
// // auto graph_6 = graph_4.do([11]);
// // auto tip_merge = graph_4.fuse(graph_5, graph_6);
// // auto final = graph_1.fuse(tip_merge, graph_3);
// 
//	std::tuple<
//		fork_token_t> // auto graph_1 = FlowGraph() // default, it is implied that empty tuples are legal
// 
//	std::tuple<
//		fork_token_t, decltype({1})> // auto graph_1 = FlowGraph().do([1])
// 
//	std::tuple<
//		fork_token_t, decltype({1})
//			std::tuple<
//				fork_token_t>>> // graph_1: // auto graph_1 = FlowGraph().do([1]).fork();
// 
//	std::tuple<
//		fork_token_t, decltype({1})
//			std::tuple<
//				fork_token_t, decltype({2})>>> // auto graph_2 = graph_1.do([2])
// 
//	std::tuple<
//		fork_token_t, decltype({1})
//			std::tuple<
//				fork_token_t, decltype({2}), decltype({3})>>> // auto graph_2 = graph_1.do([2]).then([3])
// 
//	std::tuple<
//		fork_token_t, decltype({1})
//			std::tuple<
//				fork_token_t, decltype({2}), decltype({3}), 
//				std::array<
//					std::tuple<
//						decltype({4})>>>>> // graph_2: // auto graph_2 = graph_1.do([2]).then([3]).split([4]);
// 
//	std::tuple<
//		fork_token_t, decltype({1})
//			std::tuple<
//				fork_token_t, decltype({2}), decltype({3}), 
//				std::array<
//					std::tuple<
//						decltype({4}), 
//						std::tuple<
//							fork_token_t>>>>>> // graph_4:  // auto graph_4 = graph_2.fork();
// 
//	std::tuple<
//		fork_token_t, decltype({1})
//			std::tuple<
//				fork_token_t, decltype({2}), decltype({3}), 
//				std::array<
//					std::tuple<
//						decltype({4}), 
//						std::tuple<
//							fork_token_t, decltype({9})>>>>>> // auto graph_5 = graph_4.do([9])
// 
//	std::tuple<
//		fork_token_t, decltype({1})
//			std::tuple<
//				fork_token_t, decltype({2}), decltype({3}), 
//				std::array<
//					std::tuple<
//						decltype({4}), 
//						std::tuple<
//							fork_token_t, decltype({9}), decltype({10})>>>>>> // graph_5: // auto graph_5 = graph_4.do([9]).then([10]);
// 
//	std::tuple<
//		fork_token_t, decltype({1})
//			std::tuple<
//				fork_token_t, decltype({2}), decltype({3}), 
//				std::array<
//					std::tuple<
//						decltype({4}), 
//						std::tuple<
//							fork_token_t, decltype({11})>>>>>> // graph_6: // auto graph_6 = graph_4.do([11]);
// 
//	std::tuple<
//		fork_token_t, decltype({1})
//			std::tuple<
//				fork_token_t, decltype({5})>>> // auto graph_3 = graph_1.do([5])
// 
//	std::tuple<
//		fork_token_t, decltype({1})
//			std::tuple<
//				fork_token_t, decltype({5}), decltype({6})>>> // auto graph_3 = graph_1.do([5]).then([6])
// 
//	std::tuple<
//		fork_token_t, decltype({1})
//			std::tuple<
//				fork_token_t, decltype({5}), decltype({6})
//					std::array<
//						std::tuple<
//							decltype({4})>>>>> // auto graph_3 = graph_1.do([5]).then([6]).split([4])
// 
//	std::tuple<
//		fork_token_t, decltype({1})
//			std::tuple<
//				fork_token_t, decltype({5}), decltype({6})
//					std::array<
//						std::tuple<
//							decltype({4}), decltype({8})>>>>> // graph_3: // auto graph_3 = graph_1.do([5]).then([6]).split([4]).then([8]);
// 
// 
// 
// template <typename Callable>
// auto do(Callable&& c); // adds child to the latest fork
// 

#include <vector>
#include <deque>
#include <list>
#include <unordered_set>
#include <tuple>
#include <memory>
#include <atomic>
#include <mutex>
#include <thread>
#include <utility>
#include <type_traits>
#include <cstddef>


#include "meta.hpp"

class task_pool {
	class task_executor;

	enum class task_state {
		pending, 
		schedule, 
		process, 
		done, 
		cancel, 
		displace
	};

	class task_concept {
	public:
		virtual ~task_concept() = default;

		task_concept(const task_concept& other) = delete;
		task_concept& operator=(const task_concept& other) = delete;

		task_concept() = default;

		virtual void execute(task_executor& executor) = 0;
	};

	template <typename T>
	class task_model : public task_concept {
		T content;
	public:
		task_model(T&& item) : content(std::move(item)) {}

		void execute(task_executor& executor) override {
			this->content.execute(executor);
		}
	};

	template <typename payload_t>
	class task_base {
	protected:
		meta::call_arguments_types<payload_t> payload_params;

		~task_base() = default;

		task_base(task_base&& other) = default;
		task_base& operator=(task_base&& other) = default;

		template <typename... Args>
		task_base(Args&&... args) :
			payload_params{std::forward<Args>(args)...} {}

		decltype(auto) call_payload() {
			payload_t callable; 
			// TODO: static callables? C++23 introduces static lambdas
			// maybe use SFINAE depending on accessibility of static operator()
			return std::apply(callable, std::move(this->payload_params));
		}
	};

	template <typename payload_t>
	class terminal_task: public task_base<payload_t> {
	public:
		template <typename... Args>
		terminal_task(Args&&... args) : task_base<payload_t>(std::forward<Args>(args)...) {}

		void execute(task_executor& pool) {
			this->call_payload();
		}
	};

	template <typename payload_t, typename consumer>
	class consequtive_task : public task_base<payload_t> {
	public:
		template <typename... Args>
		consequtive_task(Args&&... args) : task_base<payload_t>(std::forward<Args>(args)...) {}

		void execute(task_executor& pool) {
			pool.put_task(consumer(this->call_payload()));
		}
	};

	template <typename payload_t, typename split_consumer>
	class splitter_task : public task_base<payload_t> {
	public:
		template <typename... Args>
		splitter_task(Args&&... args) : task_base<payload_t>(std::forward<Args>(args)...) {}

		void execute(task_executor& pool) {
			auto result = this->call_payload();

			for (auto&& item : result) {
				pool.put_task(split_consumer(std::move(item)));
			}
		}
	};

	template <typename payload_t, typename... alternative_consumers>
	class fork_task : public task_base<payload_t> {
	public:
		template <typename... Args>
		fork_task(Args&&... args) : task_base<payload_t>(std::forward<Args>(args)...) {}

		void execute(task_executor& pool) {
			auto result = this->call_payload();

			// copies the result for every alternative consumer instance
			// 
			// we'd like to enforce move for the argument of the last task...
			(pool.put_task(alternative_consumers(result)), ...);
		}
	};

public:
	class flow_graph {
		template <typename T>
		struct split_token;
	
		template <typename... Ts>
		struct split_token<std::tuple<Ts...>> {
			using branch_t = std::tuple<Ts...>;
		};
	
	
		struct fuse_token {};
	
	
		template <typename... Ts>
		using task_sequence_node_t = std::tuple<Ts...>;
	
		template <typename... Ts>
		using task_split_node_t = split_token<std::tuple<Ts...>>;
	
		template <typename... Ts>
		using task_fork_node_t = std::tuple<fuse_token, Ts...>;
	
	
	
		template <typename T>
		struct is_split: public std::false_type { };
	
		template <typename T>
		struct is_split<split_token<T>>: public std::true_type { };
	
	
		template <typename T>
		struct is_fork: public std::false_type { };
	
		template <typename... Ts>
		struct is_fork<std::tuple<Ts...>>: public std::true_type { };
	
	
		template <typename T>
		struct is_special_node {
			static constexpr bool value = is_fork<T>::value || is_split<T>::value;
		};
	
	
		template <typename T, typename D>
		struct append_branch_tip;
	
		template <typename T, typename... Ts>
		struct append_branch_tip<task_split_node_t<Ts...>, T> {
		private:
			using branch_t = task_split_node_t<Ts...>::branch_t;
		public:
	
			using type = split_token<typename append_branch_tip<branch_t, T>::type>;
		};

		template <typename T, typename... Ts>
		struct append_branch_tip<std::tuple<Ts...>, T> {
		private:
			using branch_t = std::tuple<Ts...>;
			using level_tip_t = meta::tuple_element_last_t<branch_t>;
	
			template <bool is_special, typename NodeT>
			struct node_handler {
				using type = meta::tuple_append_element_t<NodeT, T>;
			};
	
			template <typename NodeT>
			struct node_handler<true, NodeT> {
				using type = meta::tuple_replace_last_element_t<NodeT, typename append_branch_tip<level_tip_t, T>::type>;
			};
	
		public:
	
			using type = node_handler<is_special_node<level_tip_t>::value, branch_t>::type;
		};
	
	
		template <typename Trunk, typename... Branches>
		struct is_same_root {
			static constexpr bool value = (std::is_same_v<
				meta::tuple_remove_last_element_t<Trunk>, 
				meta::tuple_remove_last_element_t<Branches>> && ...);
		};
	
		template <typename Trunk, typename... Branches>
		struct is_same_root<split_token<Trunk>, Branches...> {
			static constexpr bool value = (std::is_same_v<
				meta::tuple_remove_last_element_t<Trunk>, 
				meta::tuple_remove_last_element_t<typename Branches::branch_t>> && ...);
		};
	
	
		template <typename Trunk, typename... Branches>
		struct fuse_branches_onto_common_trunc {
		private:
			template <typename Root, typename... Branches>
			struct follow_common_root {
				using type = meta::tuple_replace_last_element_t<
					Root, 
					typename fuse_branches_onto_common_trunc<
						meta::tuple_element_last_t<Root>,
						meta::tuple_element_last_t<Branches>...
					>::type>;
			};
	
			template <typename Root, typename... Branches>
			struct follow_common_root<split_token<Root>, Branches...> {
				using type = split_token<
					meta::tuple_replace_last_element_t<
						Root, 
						typename fuse_branches_onto_common_trunc<
							meta::tuple_element_last_t<Root>,
							meta::tuple_element_last_t<typename Branches::branch_t>...
						>::type>>;
			};
	
	
			template <typename Root, typename... Branches>
			struct merge_branches;
	
			template <typename... Branches>
			struct merge_branches<task_fork_node_t<>, Branches...> {
				using type = std::tuple<meta::tuple_remove_first_element_t<Branches>...>;
			};
	
	
			template <bool do_follow, typename BaseTrunk, typename... DerivedBranches>
			struct follow_or_merge_mx {
				using type = merge_branches<BaseTrunk, DerivedBranches...>::type;
			};
	
			template <typename BaseTrunk, typename... DerivedBranches>
			struct follow_or_merge_mx<true, BaseTrunk, DerivedBranches...> {
				using type = follow_common_root<BaseTrunk, DerivedBranches...>::type;
			};
		
		public:
			using type = follow_or_merge_mx<is_same_root<Trunk, Branches...>::value,
				Trunk, Branches...>::type;
		};
	
	
		template<typename T>
		struct branch {
		public:
			using content = T;
	
		public:
			template <typename D>
			using then = branch<typename append_branch_tip<content, D>::type>;
	
			template <typename D>
			using split = branch<typename append_branch_tip<content, task_split_node_t<D>>::type>;
	
			using fork = branch<typename append_branch_tip<content, task_fork_node_t<>>::type>;
	
			template <typename... Branches>
			using fuse = branch<typename fuse_branches_onto_common_trunc<content, typename Branches::content...>::type>;
	
			// TODO: implement sync operation that effectively merges subtree's branches into single
			// branch. I.e. adds a node for one of the target branches with a task that waits for 
			// completion of other branches the sync op is applied to. That sync node can be passed
			// with arguments from the preceding node of the owning branch, other target branches
			// cannot pass their output to the sync node as parameters.
		};
	
	
		template <typename T>
		struct graph_parser;
	
		template <typename T>
		struct graph_parser<branch<T>> {
		private:
			template <typename T>
			struct node_parser;
	
			template <typename T>
			struct node_parser<std::tuple<T>> {
				using type = terminal_task<T>;
			};
	
			template <typename... Ts>
			struct node_parser<std::tuple<Ts...>> {
			private:
				using subject_t = std::tuple<Ts...>;
			public:
				using type = consequtive_task<
					meta::tuple_element_first_t<subject_t>, 
					typename node_parser<meta::tuple_remove_first_element_t<subject_t>>::type>;
			};
	
			template <typename T, typename... Ts>
			struct node_parser<std::tuple<T, std::tuple<Ts...>>> {
				// every item of Ts is expected to match type std::tuple<...>
				using type = fork_task<T, typename node_parser<Ts>::type...>; 
			};
	
			template <typename T, typename... Ts>
			struct node_parser<std::tuple<T, task_split_node_t<Ts...>>> {
			private:
				using subject_t = task_split_node_t<Ts...>;
			public:
				using type = splitter_task<T, typename node_parser<typename subject_t::branch_t>::type>;
			};
	
			// empty graph stub
			template <>
			struct node_parser<std::tuple<>> {
				using type = terminal_task<decltype([]() -> void {
						// empty flow graph instantiated. 
						// No tasks are going to be scheduled except this stub.
						// Probably this is a bug
						return;
					})>;
			};
	
	
			template <bool first_elem_is_fuse, typename T>
			struct remove_root_fuse_token_impl {
				using type = T;
			};
	
			template <typename... Ts>
			struct remove_root_fuse_token_impl<true, std::tuple<Ts...>> {
			private:
				using subject_t = std::tuple<Ts...>;
			public:
				using type = meta::tuple_remove_first_element_t<subject_t>;
			};
	
			template <typename T>
			using remove_root_fuse_token_t = remove_root_fuse_token_impl<
				std::is_same_v<meta::tuple_element_first_t<T>, fuse_token>, T>::type;
	
	
		private:
			using root_node_t = remove_root_fuse_token_t<typename branch<T>::content>;
		public:
			using parsed = node_parser<root_node_t>::type;
		};

	public:
		struct root : public branch<task_fork_node_t<>> {};

		template <typename T>
		using parse = graph_parser<T>::parsed;
	};

private:
	struct task_descriptor {
		// is not part of task_concept due to locality considerations
		size_t id;
		task_state state;
	};

	struct managed_task_item {
		// should have no constructors to be an aggregate
		// but should be forced to be move-only type?
		std::unique_ptr<task_concept> task;
		task_descriptor descriptor;

		explicit operator bool() {
			return this->task != nullptr;
		}
	};

public:
	~task_pool();
	task_pool();

	template <typename... Ts>
	void add_tasks(Ts&&... tasks);

	void execute_flow(bool async = false);
	
private:
	void enqueue_scheduling_task(task_executor& executor);
	bool schedule(task_executor& executor);

	// Task stealing is used in several flows: 
	//		on scheduling, in a context of a scheduling task, if global queue is empty 
	//			before end of scheduling sequence
	//		on local task fetch, if local queue is empty, in a context of a task switch
	// In both flow, custom action should be taken on every stealing step after every 
	// stolen task is completed. On steal attempt fail, new neighbor should be selected.
	// 
	//

	// implement two-level schedule queue access syncronization:
	//		sync access the whole list or the whole scheduling action (to prevent several threads from 
	//			concurrent scheduling)
	//		sync access to the last schedule queue item to allow queue population while schedule action 
	//			is in progress (to prevent excessive efficiency loss due to uneffective scheduling)
	//
	void enqueue_scheduler(std::list<managed_task_item>&& tasks) noexcept;
	void enqueue_scheduler(std::list<managed_task_item>& tasks) noexcept;

	void notify_executor_state_change() noexcept;
	std::vector<task_executor*> get_stealing_target_sequence(task_executor& initiator);

private:
	class task_generator {
	public:
		template <typename T>
		managed_task_item create_task(T&& task, task_state state = task_state::schedule);

	private:
		inline size_t generate_task_id() noexcept;

	private:
		size_t last_task_id = 0;
	};
	

	class task_executor: protected task_generator {
	public:
		enum class state : ptrdiff_t {
			init, 
			busy, 
			halted, 
			pending_shutdown, 
			shutdown
		};

		task_executor(task_pool& parent_pool);

		void start_synchronous_execution();
		void start_asynchronous_execution();
		
		template <typename T>
		void put_task(T&& task);
		void enqueue(managed_task_item&& task_item);
		void enqueue(std::list<managed_task_item>&& tasks) noexcept;

		template <typename F>
		bool steal_cycle(F&& pred);
		void handle_scheduling();

		void wake_up();
		void shutdown();

		state get_state() const;
		size_t enqueued_count() const noexcept;

	private:
		template <typename F>
		void execution_cycle(F&& on_halt_handler);

		[[nodiscard]]
		managed_task_item try_steal_task() noexcept;

		bool execute_task(managed_task_item&& task_item);

	private:
		task_pool& parent_pool;

		std::list<managed_task_item> local_execution_queue;
		mutable std::mutex execution_queue_mx;
		std::atomic<size_t> weak_execution_queue_size = 0;

		// buffer to dump locally created/forked tasks to the global schedule queue
		std::list<managed_task_item> local_schedule_queue;

		// std::vector<managed_task_item> retired_tasks;
		std::deque<task_descriptor> retired_tasks;

		std::atomic<state> current_state = state::init;
	};

private:
	mutable std::mutex schedule_queue_mx;
	std::list<managed_task_item> global_schedule_queue;
	std::vector<std::thread> threads;
	std::vector<std::unique_ptr<task_executor>> executors;
	std::unordered_map<task_executor*, ptrdiff_t> executor_index_map;

	task_generator generator;

	mutable std::mutex scheduling_mx;	// TODO: prevent false sharing?

	std::atomic<ptrdiff_t> state_subscription_counter = 0;
	std::atomic<ptrdiff_t> schedule_epoch_counter = 0;
};


// task_pool implementation section:
//

template <typename... Ts>
void task_pool::add_tasks(Ts&&... tasks) {
	std::list<managed_task_item> temp_task_buffer;
	((temp_task_buffer.push_back(this->generator.create_task(std::forward<Ts>(tasks)))), ...);
	// TODO: generator.create_task is potentially concurrent
	this->enqueue_scheduler(std::move(temp_task_buffer));
}


// task_executor implementation section:
// 

template <typename T>
void task_pool::task_executor::put_task(T&& task) {
	std::lock_guard lock(this->execution_queue_mx);
	this->local_schedule_queue.push_back(this->create_task(std::forward<T>(task)));
}
// other templated member functions are never used in other translation except tasking one


// task_generator implementation section:
// 

template <typename T>
task_pool::managed_task_item task_pool::task_generator::create_task(T&& task, task_state state) {
	return {
		std::make_unique<task_model<T>>(std::forward<T>(task)),
		{
			this->generate_task_id(),
			state
		}
	};
}

inline size_t task_pool::task_generator::generate_task_id() noexcept {
	size_t unque_seed = 0; // TODO: implement id uniqueness guarantee
	return ++(this->last_task_id);
}


// scheduling task: every scheduling cycle puts a schedule task into some executor's queue.
// Since the task is on the tip (end) of the queue, it is either ececuted by owning executor, 
// either stolen by the first thread to try stealing from executor selected for scheduling after 
// it process all it's own tasks. Every steal attempt therefore should be taken on a new executor, 
// no subsequent steal attempt on the same executor is allowed until no other available/has 
// non-empty queue (the case when we turned around to the executor that we started from is not a bug, 
// that means that some other executor is performing scheduling already, and we should keep stealing
// until our local queue becomes non-empty).
// 
// When scheduling task is performed, global schedule queue is checked. 
// 
//		If global queue is not empty, 
// all enqueued tasks are scheduled to all available executors, aiming equal number of locally 
// enqueued tasks simultaneously for all executors; it makes sense to put +1 task for every empty 
// executor if any. (possible formula: 
//		(global enqueued + (foreach + ... local enqueued)) / executors_num; 
//			(dispatch remainder also, maybe randomly) => 
//		array of tasks amount for every executor =>
//		+= array of bool describing if local queues are empty
// After number of tasks to schedule for every executor is determined, append to local queues 
// corresponding amount of tasks from the global queue. Pick random executor to start from 
// (or the executor performing the scheduling task).
// If global queue is not empty after preceding steps are performed, split tasks from global 
// queue equally between executors, start from the last processed executor. Repeat this step 
// until global queue is empty.
// When global queue is consumed, traverse all remaining executors that did not receive newly 
// scheduled tasks, if any, keeping processing iterator updated on every item; wake up any 
// executor in idle state (so that they begin stealing). 
// Put scheduling task to the last executor processed. Exit scheduling task.
// 
//		If global queue is empty, 
// as a part of the scheudling task, continue stealing from other executors' queues. Check 
// global queue after each completed task that was stolen. Repeat until global queue is not 
// empty; then perform scheduling procedure for non-empty global queue (scheduling is 
// considered successful in this case). 
// If all local queues are empty, wait until all executors are done with their current tasks.
// If the global queue remains empty after all, call schedule exhoustion handler (end of 
// sheduling system work cycle).
// 
// 
// Task stealing:
// If local queue is empty, check neigbour executor's queue, and steal its tip task if 
// available and execute it. If no tasks available in the neighbour's queue, mark the 
// neighbour as empty locally, and proceed stealing tasks. Pick new neighbour on every 
// steal attempt (either successful or not), extending neighbourhood range on every 
// attempt, but skip neighbours known to be empty from the local cache; check own queue 
// on every steal attempt completion and finish stealing if the queue is not empty 
// anymore, falling back to the regular task consumption flow. If all the neighbours
// are found to be empty, go to idle state, waiting for wake up signal on local queue 
// population or explicit wake up by scheduler. When woken up, execute normal consumption
// flow.
// 
// Pending tasks are not allowed to be scheduled to local queues, they should remain in the 
// global queue (or some dedicated container) until wait condition is satisfied.
// 
// Special task types:
// schedule task
// wake-up task: on wait condition event completion traverse the pending tasks in global queue
//		and move them to the global schedule queue. Don't perform any additional actions.
//

// Executor state change scenarios
// Two states available: busy and halted.
// Busy is the default state, state transition is performed 
//		by the executor itself when conditions to switch halted state are satisfied 
//			(local queue is populated)
//		by the scheduler, whenever explicit state change to busy is required
// busy -> halted transition is performed:
//		by the executor istelf, when conditions to switch states are satisfied 
//			(task stealing failure and local queue is empty)
//		by the scheduler when system halt is required (?)
// 
// halted -> busy may be covered by explicit state set by scheduler, on scheduling when local 
// queue is written, or in any scenario when needed.
// 
// but anyway local queue access syncronization is needed!
// as well as for global queue!
//
