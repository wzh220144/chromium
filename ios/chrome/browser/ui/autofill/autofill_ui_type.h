// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_UI_TYPE_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_UI_TYPE_H_

#import <Foundation/Foundation.h>

// Each one of the following types corresponds to an autofill::ServerFieldType.
typedef NS_ENUM(NSInteger, AutofillUIType) {
  AutofillUITypeUnknown,
  AutofillUITypeCreditCardNumber,
  AutofillUITypeCreditCardHolderFullName,
  AutofillUITypeCreditCardExpMonth,
  AutofillUITypeCreditCardExpYear,
  AutofillUITypeCreditCardBillingAddress,
  AutofillUITypeProfileFullName,
  AutofillUITypeProfileCompanyName,
  AutofillUITypeProfileHomeAddressLine1,
  AutofillUITypeProfileHomeAddressLine2,
  AutofillUITypeProfileHomeAddressCity,
  AutofillUITypeProfileHomeAddressState,
  AutofillUITypeProfileHomeAddressZip,
  AutofillUITypeProfileHomeAddressCountry,
  AutofillUITypeProfileHomePhoneWholeNumber,
  AutofillUITypeProfileEmailAddress,
};

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_UI_TYPE_H_
