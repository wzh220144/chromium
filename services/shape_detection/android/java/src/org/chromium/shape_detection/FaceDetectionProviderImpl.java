// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.shape_detection;

import android.content.Context;

import com.google.android.gms.common.ConnectionResult;
import com.google.android.gms.common.GoogleApiAvailability;

import org.chromium.mojo.bindings.InterfaceRequest;
import org.chromium.mojo.system.MojoException;
import org.chromium.services.service_manager.InterfaceFactory;
import org.chromium.shape_detection.mojom.FaceDetection;
import org.chromium.shape_detection.mojom.FaceDetectionProvider;
import org.chromium.shape_detection.mojom.FaceDetectorOptions;

/**
 * Service provider to create FaceDetection services
 */
public class FaceDetectionProviderImpl implements FaceDetectionProvider {
    private final Context mContext;

    public FaceDetectionProviderImpl(Context context) {
        mContext = context;
    }

    @Override
    public void createFaceDetection(
            InterfaceRequest<FaceDetection> request, FaceDetectorOptions options) {
        final boolean isGmsCoreSupported =
                GoogleApiAvailability.getInstance().isGooglePlayServicesAvailable(mContext)
                == ConnectionResult.SUCCESS;

        if (isGmsCoreSupported) {
            FaceDetection.MANAGER.bind(new FaceDetectionImplGmsCore(mContext, options), request);
        } else {
            FaceDetection.MANAGER.bind(new FaceDetectionImpl(options), request);
        }
    }

    @Override
    public void close() {}

    @Override
    public void onConnectionError(MojoException e) {}

    /**
     * A factory class to register FaceDetectionProvider interface.
     */
    public static class Factory implements InterfaceFactory<FaceDetectionProvider> {
        private final Context mContext;

        public Factory(Context context) {
            mContext = context;
        }

        @Override
        public FaceDetectionProvider createImpl() {
            return new FaceDetectionProviderImpl(mContext);
        }
    }
}
