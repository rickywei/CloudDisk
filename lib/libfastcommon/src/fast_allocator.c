//fast_allocator.c

#include <errno.h>
#include <stdlib.h>
#include <pthread.h>
#include "logger.h"
#include "shared_func.h"
#include "sched_thread.h"
#include "fast_allocator.h"

#define BYTES_ALIGN(x, pad_mask)  (((x) + pad_mask) & (~pad_mask))

struct allocator_wrapper {
	int alloc_bytes;
	short allocator_index;
	short magic_number;
};

static struct fast_allocator_info malloc_allocator;

#define ADD_ALLOCATOR_TO_ARRAY(acontext, allocator, _pooled) \
	do { \
		(allocator)->index = acontext->allocator_array.count; \
		(allocator)->magic_number = rand();   \
		(allocator)->pooled = _pooled;        \
		acontext->allocator_array.allocators[ \
			acontext->allocator_array.count++] = allocator; \
		/* logInfo("count: %d, magic_number: %d", acontext->allocator_array.count, (allocator)->magic_number); */\
	} while (0)


static int fast_allocator_malloc_trunk_check(const int alloc_bytes, void *args)
{
	struct fast_allocator_context *acontext;
	acontext = (struct fast_allocator_context *)args;
	if (acontext->alloc_bytes_limit == 0)
	{
		return 0;
	}

	if (acontext->alloc_bytes + alloc_bytes > acontext->alloc_bytes_limit)
	{
		return EOVERFLOW;
	}

	return acontext->allocator_array.malloc_bytes + alloc_bytes <=
		acontext->allocator_array.malloc_bytes_limit ? 0 : EOVERFLOW;
}

static void fast_allocator_malloc_trunk_notify_func(const int alloc_bytes, void *args)
{
	if (alloc_bytes > 0)
	{
		__sync_add_and_fetch(&((struct fast_allocator_context *)args)->
			allocator_array.malloc_bytes, alloc_bytes);
	}
	else
	{
		__sync_sub_and_fetch(&((struct fast_allocator_context *)args)->
			allocator_array.malloc_bytes, -1 * alloc_bytes);
	}
}

static int allocator_array_check_capacity(struct fast_allocator_context *acontext,
	const int allocator_count)
{
	int bytes;
    int target_count;
    int alloc_count;
	struct fast_allocator_info  **new_allocators;

    target_count = acontext->allocator_array.count + allocator_count;
	if (acontext->allocator_array.alloc >= target_count)
	{
		return 0;
	}
	if (acontext->allocator_array.alloc == 0)
	{
        if (target_count < 128)
        {
            alloc_count = 128;
        }
        else if (target_count < 256)
        {
            alloc_count = 256;
        }
        else if (target_count < 512)
        {
            alloc_count = 512;
        }
        else if (target_count < 1024)
        {
            alloc_count = 1024;
        }
        else
        {
            alloc_count = 2 * target_count;
        }
	}
	else
	{
        alloc_count = acontext->allocator_array.alloc;
		do
		{
			alloc_count *= 2;
		} while (alloc_count < target_count);
	}

	bytes = sizeof(struct fast_allocator_info *) * alloc_count;
	new_allocators = (struct fast_allocator_info **)fc_malloc(bytes);
	if (new_allocators == NULL)
	{
		return ENOMEM;
	}

	if (acontext->allocator_array.allocators != NULL)
	{
		memcpy(new_allocators, acontext->allocator_array.allocators,
			sizeof(struct fast_allocator_info *) *
			acontext->allocator_array.count);
		free(acontext->allocator_array.allocators);
	}
    acontext->allocator_array.alloc = alloc_count;
	acontext->allocator_array.allocators = new_allocators;
	return 0;
}

static int region_init(struct fast_allocator_context *acontext,
        const char *mblock_name_prefix, struct fast_region_info *region)
{
	int result;
	int bytes;
	int element_size;
	int allocator_count;
	struct fast_allocator_info *allocator;
    char *name;
    char name_buff[FAST_MBLOCK_NAME_SIZE];

	region->pad_mask = region->step - 1;
	allocator_count = (region->end - region->start) / region->step;
	bytes = sizeof(struct fast_allocator_info) * allocator_count;
	region->allocators = (struct fast_allocator_info *)fc_malloc(bytes);
	if (region->allocators == NULL)
	{
		return ENOMEM;
	}
	memset(region->allocators, 0, bytes);

	if ((result=allocator_array_check_capacity(acontext, allocator_count)) != 0)
	{
		return result;
	}

    name = name_buff;
	result = 0;
 	allocator = region->allocators;
	for (element_size=region->start+region->step; element_size<=region->end;
		element_size+=region->step,allocator++)
	{
        if (mblock_name_prefix != NULL)
        {
            snprintf(name, FAST_MBLOCK_NAME_SIZE, "%s-%d",
                    mblock_name_prefix, element_size);
        }
        else
        {
            name = NULL;
        }
		result = fast_mblock_init_ex2(&allocator->mblock, name, element_size,
			region->alloc_elements_once, NULL, NULL, acontext->need_lock,
			fast_allocator_malloc_trunk_check,
			fast_allocator_malloc_trunk_notify_func, acontext);
		if (result != 0)
		{
			break;
		}

		ADD_ALLOCATOR_TO_ARRAY(acontext, allocator, true);
	}

	return result;
}

static void region_destroy(struct fast_allocator_context *acontext,
	struct fast_region_info *region)
{
	int element_size;
	struct fast_allocator_info *allocator;

	allocator = region->allocators;
	for (element_size=region->start+region->step; element_size<=region->end;
		element_size+=region->step,allocator++)
	{
		fast_mblock_destroy(&allocator->mblock);
	}

	free(region->allocators);
	region->allocators = NULL;
}

int fast_allocator_init_ex(struct fast_allocator_context *acontext,
        const char *mblock_name_prefix, struct fast_region_info *regions,
        const int region_count, const int64_t alloc_bytes_limit,
        const double expect_usage_ratio, const int reclaim_interval,
        const bool need_lock)
{
	int result;
	int bytes;
	int previous_end;
	struct fast_region_info *pRegion;
	struct fast_region_info *region_end;

	srand(time(NULL));
	memset(acontext, 0, sizeof(*acontext));
	if (region_count <= 0)
	{
		return EINVAL;
	}

	bytes = sizeof(struct fast_region_info) * region_count;
	acontext->regions = (struct fast_region_info *)fc_malloc(bytes);
	if (acontext->regions == NULL)
	{
		return ENOMEM;
	}
	memcpy(acontext->regions, regions, bytes);
	acontext->region_count = region_count;
	acontext->alloc_bytes_limit = alloc_bytes_limit;
	if (expect_usage_ratio < 0.01 || expect_usage_ratio > 1.00)
	{
		acontext->allocator_array.expect_usage_ratio = 0.80;
	}
	else
	{
		acontext->allocator_array.expect_usage_ratio = expect_usage_ratio;
	}
	acontext->allocator_array.malloc_bytes_limit = alloc_bytes_limit /
		acontext->allocator_array.expect_usage_ratio;
	acontext->allocator_array.reclaim_interval = reclaim_interval;
	acontext->need_lock = need_lock;
	result = 0;
	previous_end = 0;
	region_end = acontext->regions + acontext->region_count;
	for (pRegion=acontext->regions; pRegion<region_end; pRegion++)
	{
		if (pRegion->start != previous_end)
		{
			logError("file: "__FILE__", line: %d, "
				"invalid start: %d != last end: %d",
				__LINE__, pRegion->start, previous_end);
			result = EINVAL;
			break;
		}
		if (pRegion->start >= pRegion->end)
		{
			logError("file: "__FILE__", line: %d, "
				"invalid start: %d >= end: %d",
				__LINE__, pRegion->start, pRegion->end);
			result = EINVAL;
			break;
		}
		if (pRegion->step <= 0 || !is_power2(pRegion->step))
		{
			logError("file: "__FILE__", line: %d, "
				"invalid step: %d",
				__LINE__, pRegion->step);
			result = EINVAL;
			break;
		}
		if (pRegion->start % pRegion->step != 0)
		{
			logError("file: "__FILE__", line: %d, "
				"invalid start: %d, must multiple of step: %d",
				__LINE__, pRegion->start, pRegion->step);
			result = EINVAL;
			break;
		}
		if (pRegion->end % pRegion->step != 0)
		{
			logError("file: "__FILE__", line: %d, "
				"invalid end: %d, must multiple of step: %d",
				__LINE__, pRegion->end, pRegion->step);
			result = EINVAL;
			break;
		}
		previous_end = pRegion->end;

		if ((result=region_init(acontext, mblock_name_prefix, pRegion)) != 0)
		{
			break;
		}
	}

	if (result != 0)
	{
		return result;
	}

	if ((result=allocator_array_check_capacity(acontext, 1)) != 0)
	{
		return result;
	}

	ADD_ALLOCATOR_TO_ARRAY(acontext, &malloc_allocator, false);
	/*
	logInfo("sizeof(struct allocator_wrapper): %d, allocator_array count: %d",
		(int)sizeof(struct allocator_wrapper), acontext->allocator_array.count);
	*/
	return result;
}

int fast_allocator_init(struct fast_allocator_context *acontext,
        const char *mblock_name_prefix, const int64_t alloc_bytes_limit,
        const double expect_usage_ratio, const int reclaim_interval,
        const bool need_lock)
{
#define DEFAULT_REGION_COUNT 5

    struct fast_region_info regions[DEFAULT_REGION_COUNT];

    FAST_ALLOCATOR_INIT_REGION(regions[0],     0,   256,    8, 4096);
    FAST_ALLOCATOR_INIT_REGION(regions[1],   256,  1024,   16, 1024);
    FAST_ALLOCATOR_INIT_REGION(regions[2],  1024,  4096,   64,  256);
    FAST_ALLOCATOR_INIT_REGION(regions[3],  4096, 16384,  256,   64);
    FAST_ALLOCATOR_INIT_REGION(regions[4], 16384, 65536, 1024,   16);

    return fast_allocator_init_ex(acontext, mblock_name_prefix, regions,
            DEFAULT_REGION_COUNT, alloc_bytes_limit, expect_usage_ratio,
            reclaim_interval, need_lock);
}

void fast_allocator_destroy(struct fast_allocator_context *acontext)
{
	struct fast_region_info *pRegion;
	struct fast_region_info *region_end;

	if (acontext->regions != NULL)
	{
		region_end = acontext->regions + acontext->region_count;
		for (pRegion=acontext->regions; pRegion<region_end; pRegion++)
		{
			region_destroy(acontext, pRegion);
		}
		free(acontext->regions);
	}

	if (acontext->allocator_array.allocators != NULL)
	{
		free(acontext->allocator_array.allocators);
	}
	memset(acontext, 0, sizeof(*acontext));
}

static struct fast_allocator_info *get_allocator(struct fast_allocator_context *acontext,
	int *alloc_bytes)
{
	struct fast_region_info *pRegion;
	struct fast_region_info *region_end;

	region_end = acontext->regions + acontext->region_count;
	for (pRegion=acontext->regions; pRegion<region_end; pRegion++)
	{
		if (*alloc_bytes <= pRegion->end)
		{
			*alloc_bytes = BYTES_ALIGN(*alloc_bytes, pRegion->pad_mask);
			return pRegion->allocators + ((*alloc_bytes -
				pRegion->start) / pRegion->step) - 1;
		}
	}

	return &malloc_allocator;
}

int fast_allocator_retry_reclaim(struct fast_allocator_context *acontext,
	int64_t *total_reclaim_bytes)
{
	int64_t malloc_bytes;
	int reclaim_count;
	int i;

	*total_reclaim_bytes = 0;
	if (acontext->allocator_array.last_reclaim_time +
		acontext->allocator_array.reclaim_interval > get_current_time())
	{
		return EAGAIN;
	}

	acontext->allocator_array.last_reclaim_time = get_current_time();
	malloc_bytes = acontext->allocator_array.malloc_bytes;
	logInfo("malloc_bytes: %"PRId64", ratio: %f", malloc_bytes, (double)acontext->alloc_bytes /
		(double)malloc_bytes);

	if (malloc_bytes == 0 || (double)acontext->alloc_bytes /
		(double)malloc_bytes >= acontext->allocator_array.expect_usage_ratio)
	{
		return EAGAIN;
	}

	for (i=0; i< acontext->allocator_array.count; i++)
	{
		if (fast_mblock_reclaim(&acontext->allocator_array.
			allocators[i]->mblock, 0, &reclaim_count, NULL) == 0)
		{
			logInfo("reclaim_count: %d", reclaim_count);
			*total_reclaim_bytes += reclaim_count *
				acontext->allocator_array.allocators[i]->
					mblock.info.trunk_size;
		}
	}

	return *total_reclaim_bytes > 0 ? 0 : EAGAIN;
}

void *fast_allocator_alloc(struct fast_allocator_context *acontext,
	const int bytes)
{
	int alloc_bytes;
	int64_t total_reclaim_bytes;
	struct fast_allocator_info *allocator_info;
	void *ptr;

	if (bytes < 0)
	{
		return NULL;
	}

	alloc_bytes = sizeof(struct allocator_wrapper) + bytes;
	allocator_info = get_allocator(acontext, &alloc_bytes);
	if (allocator_info->pooled)
	{
		ptr = fast_mblock_alloc_object(&allocator_info->mblock);
		if (ptr == NULL)
		{
			if (acontext->allocator_array.reclaim_interval <= 0)
			{
				return NULL;
			}
			if (fast_allocator_retry_reclaim(acontext, &total_reclaim_bytes) != 0)
			{
				return NULL;
			}
			logInfo("reclaimed bytes: %"PRId64, total_reclaim_bytes);
			if (total_reclaim_bytes < allocator_info->mblock.info.trunk_size)
			{
				return NULL;
			}
			ptr = fast_mblock_alloc_object(&allocator_info->mblock);
			if (ptr == NULL)
			{
				return NULL;
			}
		}
	}
	else
	{
		if (fast_allocator_malloc_trunk_check(alloc_bytes, acontext) != 0)
		{
			return NULL;
		}
		ptr = fc_malloc(alloc_bytes);
		if (ptr == NULL)
		{
			return NULL;
		}
		fast_allocator_malloc_trunk_notify_func(alloc_bytes, acontext);
	}

	((struct allocator_wrapper *)ptr)->allocator_index = allocator_info->index;
	((struct allocator_wrapper *)ptr)->magic_number = allocator_info->magic_number;
	((struct allocator_wrapper *)ptr)->alloc_bytes = alloc_bytes;

	__sync_add_and_fetch(&acontext->alloc_bytes, alloc_bytes);
	return (char *)ptr + sizeof(struct allocator_wrapper);
}

void fast_allocator_free(struct fast_allocator_context *acontext, void *ptr)
{
	struct allocator_wrapper *pWrapper;
	struct fast_allocator_info *allocator_info;
	void *obj;
	if (ptr == NULL)
	{
		return;
	}

	obj = (char *)ptr - sizeof(struct allocator_wrapper);
	pWrapper = (struct allocator_wrapper *)obj;
	if (pWrapper->allocator_index < 0 || pWrapper->allocator_index >=
		acontext->allocator_array.count)
	{
		logError("file: "__FILE__", line: %d, "
				"invalid allocator index: %d",
				__LINE__, pWrapper->allocator_index);
		return;
	}

	allocator_info = acontext->allocator_array.allocators[pWrapper->allocator_index];
	if (pWrapper->magic_number != allocator_info->magic_number)
	{
		logError("file: "__FILE__", line: %d, "
				"invalid magic number: %d != %d",
				__LINE__, pWrapper->magic_number,
				allocator_info->magic_number);
		return;
	}

	__sync_sub_and_fetch(&acontext->alloc_bytes, pWrapper->alloc_bytes);
	pWrapper->allocator_index = -1;
	pWrapper->magic_number = 0;
	if (allocator_info->pooled)
	{
		fast_mblock_free_object(&allocator_info->mblock, obj);
	}
	else
	{
		fast_allocator_malloc_trunk_notify_func(-1 * pWrapper->alloc_bytes, acontext);
		free(obj);
	}
}

char *fast_allocator_memdup(struct fast_allocator_context *acontext,
        const char *src, const int len)
{
    char *dest;
    dest = (char *)fast_allocator_alloc(acontext, len);
    if (dest == NULL) {
        logError("file: "__FILE__", line: %d, "
                "malloc %d bytes fail", __LINE__, len);
        return NULL;
    }

    memcpy(dest, src, len);
    return dest;
}
