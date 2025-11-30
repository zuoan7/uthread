#ifndef MY_UTHREAD_CPP
#define MY_UTHREAD_CPP


#include "uthread.h"

/**
 * 恢复指定协程的执行（或启动新协程）
 * @param schedule 协程调度器引用
 * @param id 要恢复的协程ID
 */
void uthread_resume(schedule_t &schedule , int id)		
{	
	// 参数校验：确保协程ID在有效范围内
    if(id < 0 || id >= schedule.max_index){
        return;
    }

	// 获取指定ID的协程控制块
    uthread_t *t = &(schedule.threads[id]);

	 // 只能恢复处于挂起或可运行状态的协程
    if (t->state == SUSPEND||t->state==RUNNABLE) {
		// 更新调度器状态：标记当前运行的协程ID
		schedule.running_thread=id;	
		// 更新协程状态：设置为运行中
    	t->state=RUNNING;

		// 执行上下文切换：从主上下文切换到协程上下文
        // 这会保存主线程的当前状态到schedule.main，并恢复协程t->ctx的状态
        swapcontext(&(schedule.main),&(t->ctx));
    }
}

/**
 * 当前协程主动让出CPU控制权
 * @param schedule 协程调度器引用
 */
void uthread_yield(schedule_t &schedule)
{
	// 检查当前是否有协程在运行
    if(schedule.running_thread != -1 ){
		 // 获取当前运行协程的控制块
        uthread_t *t = &(schedule.threads[schedule.running_thread]);

		// 更新协程状态：从运行中变为挂起
        t->state = SUSPEND;
		// 更新调度器状态：标记当前没有协程运行
        schedule.running_thread = -1;

        // 执行上下文切换：从协程上下文切换回主上下文
        // 这会保存协程的当前状态到t->ctx，并恢复主线程schedule.main的状态
		swapcontext(&(t->ctx),&(schedule.main));
    }
}
/**
 * 协程的入口包装函数（所有协程的实际执行起点）
 * @param ps 指向调度器的指针
 * 
 * 注意：这个函数由makecontext设置，作为所有协程的统一入口点
 * 它负责调用用户真正的业务函数，并在函数结束后清理协程资源
 */
void uthread_body(schedule_t *ps)
{
	// 获取当前正在运行的协程ID
    int id = ps->running_thread;

    if(id != -1){
		// 获取当前协程的控制块
        uthread_t *t = &(ps->threads[id]);

		// 执行用户注册的业务函数，并传递参数
        t->func(t->arg);

		// 用户函数执行完毕，标记协程为空闲状态（可重用）
        t->state = FREE;

		// 更新调度器：当前没有协程在运行
        ps->running_thread = -1;
    }
	// 函数结束后，由于uc_link指向schedule.main，会自动切换回主上下文
}

/**
 * 创建新的协程
 * @param schedule 协程调度器引用
 * @param func 协程要执行的函数指针
 * @param arg 传递给协程函数的参数
 * @return 新创建协程的ID，失败返回-1
 */

int uthread_create(schedule_t &schedule,Fun func,void *arg)
{
    int id = 0;

	// 在协程数组中寻找空闲的槽位
    for(id = 0; id < schedule.max_index; ++id ){
        if(schedule.threads[id].state == FREE){
            break;
        }
    }
	
    // 如果没有找到空闲槽位，就在数组末尾分配新的槽位
    if (id == schedule.max_index) {
        schedule.max_index++;
    }

	// 获取新协程的控制块
    uthread_t *t = &(schedule.threads[id]);

	// 初始化协程状态和函数信息
    t->state = RUNNABLE;  // 设置为可运行状态
    t->func = func;       // 设置用户函数
    t->arg = arg;         // 设置函数参数

	// 获取当前上下文，作为协程上下文的基础
    getcontext(&(t->ctx));
    
   // 设置协程的栈信息
    t->ctx.uc_stack.ss_sp = t->stack;        // 栈指针指向协程的独立栈空间
    t->ctx.uc_stack.ss_size = DEFAULT_STACK_SIZE; // 栈大小
    t->ctx.uc_stack.ss_flags = 0;            // 栈标志（无特殊标志）

	// 设置上下文链接：当协程函数执行完毕后，自动切换到的上下文
    // 这里设置为调度器的主上下文，即协程结束后自动回到调度器
	t->ctx.uc_link = &(schedule.main);

	// 修改上下文，使其执行时跳转到uthread_body函数
    // 参数说明：
    // - &(t->ctx): 要修改的上下文
    // - (void (*)(void))(uthread_body): 函数指针转换
    // - 1: 参数个数
    // - &schedule: 传递给uthread_body的参数（调度器指针）
    makecontext(&(t->ctx),(void (*)(void))(uthread_body),1,&schedule);
    
    return id;	// 返回新协程的ID
}

/**
 * 检查调度器中所有协程是否都已执行完成
 * @param schedule 协程调度器常量引用
 * @return 1-所有协程都已完成，0-还有协程未完成
 */
int schedule_finished(const schedule_t &schedule)
{
	// 如果有协程正在运行，则调度肯定未完成
    if (schedule.running_thread != -1){
        return 0;
    }else{
		// 遍历所有可能存在的协程槽位
        for(int i = 0; i < schedule.max_index; ++i){
			// 如果发现任何非空闲状态的协程，说明还有协程未完成
            if(schedule.threads[i].state != FREE){
                return 0;
            }
        }
    }
	// 所有协程都是空闲状态，调度完成
    return 1;
}

#endif
