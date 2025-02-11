// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler;

import android.annotation.TargetApi;
import android.content.Context;
import android.os.Build;
import android.support.annotation.Nullable;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;

import java.lang.reflect.Constructor;
import java.util.Set;

/**
 * A BackgroundTaskScheduler which is used to schedule jobs that run in the background.
 * It is backed by system APIs ({@link android.app.job.JobScheduler}) on newer platforms
 * and by GCM ({@link com.google.android.gms.gcm.GcmNetworkManager}) on older platforms.
 *
 * To get an instance of this class, use {@link BackgroundTaskSchedulerFactory#getScheduler()}.
 */
@TargetApi(Build.VERSION_CODES.LOLLIPOP)
public class BackgroundTaskScheduler {
    private static final String TAG = "BkgrdTaskScheduler";

    @Nullable
    static BackgroundTask getBackgroundTaskFromClassName(String backgroundTaskClassName) {
        if (backgroundTaskClassName == null) return null;

        Class<?> clazz;
        try {
            clazz = Class.forName(backgroundTaskClassName);
        } catch (ClassNotFoundException e) {
            Log.w(TAG, "Unable to find BackgroundTask class with name " + backgroundTaskClassName);
            return null;
        }

        if (!BackgroundTask.class.isAssignableFrom(clazz)) {
            Log.w(TAG, "Class " + clazz + " is not a BackgroundTask");
            return null;
        }

        try {
            return (BackgroundTask) clazz.newInstance();
        } catch (InstantiationException | IllegalAccessException e) {
            Log.w(TAG, "Unable to instantiate class " + clazz);
            return null;
        }
    }

    static boolean hasParameterlessPublicConstructor(Class<? extends BackgroundTask> clazz) {
        for (Constructor<?> constructor : clazz.getConstructors()) {
            if (constructor.getParameterTypes().length == 0) return true;
        }
        return false;
    }

    private final BackgroundTaskSchedulerDelegate mSchedulerDelegate;

    BackgroundTaskScheduler(BackgroundTaskSchedulerDelegate schedulerDelegate) {
        assert schedulerDelegate != null;
        mSchedulerDelegate = schedulerDelegate;
    }

    /**
     * Schedules a background task. See {@link TaskInfo} for information on what types of tasks that
     * can be scheduled.
     *
     * @param context the current context.
     * @param taskInfo the information about the task to be scheduled.
     * @return true if the schedule operation succeeded, and false otherwise.
     * @see TaskInfo
     */
    public boolean schedule(Context context, TaskInfo taskInfo) {
        ThreadUtils.assertOnUiThread();
        boolean success = mSchedulerDelegate.schedule(context, taskInfo);
        if (success) {
            BackgroundTaskSchedulerPrefs.addScheduledTask(taskInfo);
        }
        return success;
    }

    /**
     * Cancels the task specified by the task ID.
     *
     * @param context the current context.
     * @param taskId the ID of the task to cancel. See {@link TaskIds} for a list.
     */
    public void cancel(Context context, int taskId) {
        ThreadUtils.assertOnUiThread();
        BackgroundTaskSchedulerPrefs.removeScheduledTask(taskId);
        mSchedulerDelegate.cancel(context, taskId);
    }

    public void reschedule(Context context) {
        Set<String> scheduledTasksClassNames = BackgroundTaskSchedulerPrefs.getScheduledTasks();
        BackgroundTaskSchedulerPrefs.removeAllTasks();
        for (String className : scheduledTasksClassNames) {
            BackgroundTask task = getBackgroundTaskFromClassName(className);
            if (task == null) {
                Log.w(TAG, "Cannot reschedule task for: " + className);
                continue;
            }

            task.reschedule(context);
        }
    }
}
