// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget.bottomsheet;

import org.chromium.chrome.browser.compositor.layouts.LayoutManagerChrome;
import org.chromium.chrome.browser.omnibox.LocationBarPhone;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.BottomToolbarPhone;

/**
 * A class that handles showing and hiding the Chrome Home new tab UI.
 *
 * TODO(twellington): Add tests for this class.
 */
public class BottomSheetNewTabController extends EmptyBottomSheetObserver {
    private final BottomSheet mBottomSheet;
    private final BottomToolbarPhone mToolbar;
    private final LocationBarPhone mLocationBar;

    private LayoutManagerChrome mLayoutManager;
    private TabModelSelector mTabModelSelector;

    private boolean mIsShowingNewTab;

    /**
     * Creates a new {@link BottomSheetNewTabController}.
     * @param bottomSheet The {@link BottomSheet} that will be opened as part of the new tab UI.
     * @param toolbar The {@link BottomToolbarPhone} that this controller will set state on as part
     *                of the new tab UI.
     */
    public BottomSheetNewTabController(BottomSheet bottomSheet, BottomToolbarPhone toolbar) {
        mBottomSheet = bottomSheet;
        mBottomSheet.addObserver(this);
        mToolbar = toolbar;
        mLocationBar = (LocationBarPhone) toolbar.getLocationBar();
    }

    /**
     * @param tabModelSelector A TabModelSelector for getting the current tab and activity.
     */
    public void setTabModelSelector(TabModelSelector tabModelSelector) {
        mTabModelSelector = tabModelSelector;
    }

    /**
     * @param layoutManager The {@link LayoutManagerChrome} used to show and hide overview mode.
     */
    public void setLayoutManagerChrome(LayoutManagerChrome layoutManager) {
        mLayoutManager = layoutManager;
    }

    /**
     * Shows the new tab UI.
     * @param isIncognito Whether to display the incognito new tab UI.
     */
    public void displayNewTabUi(boolean isIncognito) {
        mIsShowingNewTab = true;

        // Select the correct model, immediately ending animations so that the sheet content is not
        // in transition while the sheet is opening.
        if (mTabModelSelector.isIncognitoSelected() != isIncognito) {
            mTabModelSelector.selectModel(isIncognito);
            mBottomSheet.endTransitionAnimations();
        }

        // Update the loading status so that the location bar will update its contents.
        mLocationBar.updateLoadingState(true);

        // Open the sheet.
        mBottomSheet.setSheetState(mTabModelSelector.getTotalTabCount() == 0
                        ? BottomSheet.SHEET_STATE_FULL
                        : BottomSheet.SHEET_STATE_HALF,
                true);

        // Show the tab switcher if needed.
        if (!mLayoutManager.overviewVisible()) mLayoutManager.showOverview(true);

        mBottomSheet.getBottomSheetMetrics().recordSheetOpenReason(
                BottomSheetMetrics.OPENED_BY_NEW_TAB_CREATION);
    }

    /**
     * Called when the activity containing the {@link BottomSheet} processes a url view intent.
     * The new tab UI will be hidden.
     */
    public void onProcessUrlViewIntent() {
        if (!mIsShowingNewTab) return;

        mIsShowingNewTab = false;
        mLayoutManager.hideOverview(true);
    }

    @Override
    public void onLoadUrl(String url) {
        onProcessUrlViewIntent();
    }

    /**
     * @return Whether the the new tab UI is showing.
     */
    public boolean isShowingNewTab() {
        return mIsShowingNewTab;
    }

    @Override
    public void onSheetOpened() {
        if (!mIsShowingNewTab) return;

        // Transition from the tab switcher toolbar to the normal toolbar.
        mToolbar.showNormalToolbar();

        // ToolbarPhone makes the url bar unfocusable while in the tab switcher; make sure it is
        // focusable when the sheet is open.
        mLocationBar.setUrlBarFocusable(true);
    }

    @Override
    public void onSheetReleased() {
        if (!mIsShowingNewTab) return;

        // Start transitioning back to the tab switcher toolbar when the sheet is released to help
        // smooth out animations.
        if (mBottomSheet.getTargetSheetState() == BottomSheet.SHEET_STATE_PEEK) {
            mToolbar.showTabSwitcherToolbar();
        }
    }

    @Override
    public void onSheetClosed() {
        if (!mIsShowingNewTab) return;

        mIsShowingNewTab = false;

        // If the incognito tab model is showing, but has no tabs, this indicates that the model
        // was switched during the creation of an incognito ntp and the user closed the sheet
        // without loading a URL. Switch back to the normal model.
        if (mTabModelSelector.isIncognitoSelected()
                && mTabModelSelector.getModel(true).getCount() == 0) {
            mTabModelSelector.selectModel(false);
        }

        // Transition back to the tab switcher toolbar if the tab switcher is sill visible.
        if (mLayoutManager.overviewVisible()) mToolbar.showTabSwitcherToolbar();
    }
}
