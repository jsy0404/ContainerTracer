/**
 * @copyright "Container Tracer" which executes the container performance mesurements
 * Copyright (C) 2020 SuhoSon
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * @file docker-info.c
 * @brief Initialize the info structure.
 * @author SuhoSon (ngeol564@gmail.com)
 * @version 0.1.2
 * @date 2020-08-19
 */

#include <stdlib.h>
#include <search.h>
#include <assert.h>
#include <sys/stat.h>

#include <json.h>
#include <jemalloc/jemalloc.h>

#include <driver/docker-driver.h>

/**
 * @brief Definition of a well-known synthetic form.
 * This value depends on the `trace-replay` specification.
 */
static const char *global_synth_type[] = { "rand_read",  "rand_write",
                                           "rand_mixed", "seq_read",
                                           "seq_write",  "seq_mixed",
                                           NULL };

/**
 * @brief Read the JSON string and convert the value to integer form and set that value to `info->(member)`
 *
 * @param[in] setting The traverse start location of JSON object.
 * @param[in] key JSON object's key which points to the value I want to find.
 * @param[out] member `info` structure member address.
 * @param[in] is_print The flag that determines to print the error.
 *
 * @return 0 for success to input, -EINVAL for fail to input.
 * @warning member must be an integer type.
 */
static int docker_info_int_value_set(struct json_object *setting,
                                     const char *key, unsigned int *member,
                                     int is_print)
{
        struct json_object *tmp = NULL;
        if (!json_object_object_get_ex(setting, key, &tmp)) {
                if (DOCKER_ERROR_PRINT == is_print) {
                        pr_info(ERROR, "Not exist error (key: %s)\n", key);
                }
                return -EINVAL;
        }
        *member = json_object_get_int(tmp);
        return 0;
}

/**
 * @brief Read the JSON string and convert the value to string form and set that value to `info->(member)`
 *
 * @param[in] setting The traverse start location of JSON object.
 * @param[in] key JSON object's key which points to the value I want to find.
 * @param[out] member `info` structure member address.
 * @param[in] size This value must under `member` memory size
 * @param[in] is_print The flag that determines to print the error.
 *
 * @return 0 for success to input, -EINVAL for fail to input.
 * @warning member must be an string type.
 */
static int docker_info_str_value_set(struct json_object *setting,
                                     const char *key, char *member, size_t size,
                                     int is_print)
{
        struct json_object *tmp = NULL;
        if (!json_object_object_get_ex(setting, key, &tmp)) {
                if (DOCKER_ERROR_PRINT == is_print) {
                        pr_info(ERROR, "Not exist error (key: %s)\n", key);
                }
                return -EINVAL;
        }
        snprintf(member, size, "%s", json_object_get_string(tmp));
        generic_strip_string(member, '\"');
        return 0;
}

/**
 * @brief Check the `trace_data_path` value form is synthetic from.
 *
 * @param[in] trace_data_path The value which I want to know either synthetic or not.
 *
 * @return DOCKER_SYNTH for `trace_data_path` is synthetic,
 * DOCKER_NOT_SYNTH for `trace_data_path` isn't synthetic
 */
int docker_is_synth_type(const char *trace_data_path)
{
        int i = 0;
        for (i = 0; global_synth_type[i] != NULL; i++) {
                if (!strcmp(trace_data_path, global_synth_type[i])) {
                        return DOCKER_SYNTH;
                }
        }
        return DOCKER_NOT_SYNTH;
}

/**
 * @brief Set the configuration of each process's behavior.
 *
 * @param[in] setting JSON object pointer which has the setting value.
 * @param[in] index `task_option` array's index.
 * @param[out] info The target structure of the member will be set by the JSON object.
 *
 * @return 0 for success to init, error number for fail to init
 */
static int __docker_info_init(struct json_object *setting, int index,
                              struct docker_info *info)
{
        struct json_object *tmp;
        struct stat lstat_info;
        int ret = 0;
        int print_flag = DOCKER_PRINT_NONE;

        ENTRY item; /**< Variable for `hsearch`. */
        ENTRY *result;

        assert(NULL != info);

        if (!json_object_object_get_ex(setting, "task_option", &tmp)) {
                pr_info(ERROR, "Not exist error (key: %s)\n", "task_option");
                ret = -EINVAL;
                goto exception;
        }

        tmp = json_object_array_get_idx(tmp, index);
        if (NULL == tmp) {
                pr_info(ERROR, "Array out-of-bound error (index: %d)\n", index);
                ret = -EINVAL;
                goto exception;
        }

        docker_info_int_value_set(tmp, "time", &info->time, DOCKER_PRINT_NONE);
        docker_info_int_value_set(tmp, "q_depth", &info->q_depth,
                                  DOCKER_PRINT_NONE);
        docker_info_int_value_set(tmp, "nr_thread", &info->nr_thread,
                                  DOCKER_PRINT_NONE);
        docker_info_int_value_set(tmp, "weight", &info->weight,
                                  DOCKER_PRINT_NONE);
        docker_info_int_value_set(tmp, "trace_repeat", &info->weight,
                                  DOCKER_PRINT_NONE);
        docker_info_int_value_set(tmp, "wss", &info->wss, DOCKER_PRINT_NONE);
        docker_info_int_value_set(tmp, "utilization", &info->utilization,
                                  DOCKER_PRINT_NONE);
        docker_info_int_value_set(tmp, "iosize", &info->iosize,
                                  DOCKER_PRINT_NONE);
        docker_info_str_value_set(tmp, "prefix_cgroup_name",
                                  info->prefix_cgroup_name,
                                  sizeof(info->prefix_cgroup_name),
                                  DOCKER_PRINT_NONE);
        docker_info_str_value_set(tmp, "scheduler", info->scheduler,
                                  sizeof(info->scheduler), DOCKER_PRINT_NONE);
        docker_info_str_value_set(tmp, "trace_replay_path",
                                  info->trace_replay_path,
                                  sizeof(info->trace_replay_path),
                                  DOCKER_PRINT_NONE);
        docker_info_str_value_set(tmp, "device", info->device,
                                  sizeof(info->device), DOCKER_PRINT_NONE);
        ret = docker_valid_scheduler_test(info->scheduler);
        if (0 > ret) {
                pr_info(ERROR, "Unsupported scheduler (name: %s)\n",
                        info->scheduler);
                goto exception;
        }

        if (docker_has_weight_scheduler(ret)) {
                print_flag = DOCKER_ERROR_PRINT;
        }

        ret = docker_info_int_value_set(tmp, "weight", &info->weight,
                                        print_flag);
        if (DOCKER_ERROR_PRINT == print_flag && 0 != ret) {
                goto exception;
        }

        ret = docker_info_str_value_set(tmp, "trace_data_path",
                                        info->trace_data_path,
                                        sizeof(info->trace_data_path),
                                        DOCKER_ERROR_PRINT);
        if (0 != ret) {
                goto exception;
        }

        ret = docker_info_str_value_set(tmp, "cgroup_id", info->cgroup_id,
                                        sizeof(info->cgroup_id),
                                        DOCKER_ERROR_PRINT);
        if (0 != ret) {
                goto exception;
        }

        if (DOCKER_SYNTH != docker_is_synth_type(info->trace_data_path)) {
                if (-1 == (ret = lstat(info->trace_data_path, &lstat_info))) {
                        pr_info(ERROR, "Trace data file not exist: %s\n",
                                info->trace_data_path);
                        goto exception;
                } else {
                        pr_info(INFO, "Trace data file exist: %s\n",
                                info->trace_data_path);
                }
        }
        info->ppid = getpid();

        item.key = info->cgroup_id;
        if (NULL != (result = hsearch(item, FIND))) {
                pr_info(ERROR, "Duplicate c-group name detected (name: %s)\n",
                        result->key);
                ret = -EINVAL;
                goto exception;
        }

        item.key = info->cgroup_id;
        item.data = info;
        hsearch(item, ENTER);

exception:
        return ret;
}

/**
 * @brief __Generate__ and construct the per processes `info` object and return it.
 *
 * @param[in] setting JSON object pointer which has the setting value.
 * @param[in] index `task_option` array's index.
 *
 * @return 0 for success to init, error number for fail to init
 */
struct docker_info *docker_info_init(struct json_object *setting, int index)
{
        struct docker_info *info;
        struct stat lstat_info;
        int ret = 0;

        info = (struct docker_info *)malloc(sizeof(struct docker_info));
        if (!info) {
                pr_info(ERROR, "Memory allocation fail (name: %s)\n", "info");
                ret = -ENOMEM;
                goto exception;
        }

        memset(info, 0, sizeof(struct docker_info));
        info->trace_repeat = 1;

        ret |= docker_info_int_value_set(setting, "time", &info->time,
                                         DOCKER_ERROR_PRINT);
        ret |= docker_info_int_value_set(setting, "q_depth", &info->q_depth,
                                         DOCKER_ERROR_PRINT);
        ret |= docker_info_int_value_set(setting, "nr_thread", &info->nr_thread,
                                         DOCKER_ERROR_PRINT);
        ret |= docker_info_str_value_set(setting, "prefix_cgroup_name",
                                         info->prefix_cgroup_name,
                                         sizeof(info->prefix_cgroup_name),
                                         DOCKER_ERROR_PRINT);
        ret |= docker_info_str_value_set(setting, "scheduler", info->scheduler,
                                         sizeof(info->scheduler),
                                         DOCKER_ERROR_PRINT);
        ret |= docker_info_str_value_set(setting, "device", info->device,
                                         sizeof(info->device),
                                         DOCKER_ERROR_PRINT);
        ret |= docker_info_str_value_set(setting, "trace_replay_path",
                                         info->trace_replay_path,
                                         sizeof(info->trace_replay_path),
                                         DOCKER_ERROR_PRINT);
        if (0 != ret) {
                pr_info(ERROR, "error detected (errno: %d)\n", ret);
                goto exception;
        }

        if (-1 == lstat(info->trace_replay_path, &lstat_info)) {
                char *buffer = (char *)malloc(sizeof(info->trace_replay_path));
                assert(NULL != buffer);
                sprintf(buffer, "%s", info->trace_replay_path);
                sprintf(info->trace_replay_path, "/usr/bin/%s", buffer);
                pr_info(WARNING, "redirect: %s => %s\n", buffer,
                        info->trace_replay_path);
                free(buffer);
        }

        if (-1 == lstat(info->trace_replay_path, &lstat_info)) {
                pr_info(ERROR, "Cannot find the trace_replay_path: %s\n",
                        info->trace_replay_path);
                goto exception;
        }

        ret = docker_valid_scheduler_test(info->scheduler);
        if (0 > ret) {
                pr_info(ERROR, "Unsupported scheduler (name: %s)\n",
                        info->scheduler);
                goto exception;
        }

        docker_info_int_value_set(setting, "weight", &info->weight,
                                  DOCKER_PRINT_NONE);
        docker_info_int_value_set(setting, "trace_repeat", &info->weight,
                                  DOCKER_PRINT_NONE);
        docker_info_int_value_set(setting, "wss", &info->wss,
                                  DOCKER_PRINT_NONE);
        docker_info_int_value_set(setting, "utilization", &info->utilization,
                                  DOCKER_PRINT_NONE);
        docker_info_int_value_set(setting, "iosize", &info->iosize,
                                  DOCKER_PRINT_NONE);
        /* Validation check of `trace_data_path` in `__docker_info_init()` */
        docker_info_str_value_set(setting, "trace_data_path",
                                  info->trace_data_path,
                                  sizeof(info->trace_data_path),
                                  DOCKER_PRINT_NONE);

        ret = __docker_info_init(setting, index, info);
        if (0 != ret) {
                pr_info(ERROR, "error detected (errno: %d)\n", ret);
                goto exception;
        }

        info->mqid = info->semid = info->shmid = -1;

        info->next = NULL;

        return info;
exception:
        if (NULL != info) {
                free(info);
                info = NULL;
        }
        return info;
}
