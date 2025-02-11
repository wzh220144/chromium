// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/ios/client_keyboard.h"

// TODO(nicholss): Look into inputAccessoryView to get the top bar for sending
// special keys.
// TODO(nicholss): Look into inputView - The custom input view to display when
// the receiver becomes the first responder

@implementation ClientKeyboard

@synthesize autocapitalizationType = _autocapitalizationType;
@synthesize autocorrectionType = _autocorrectionType;
@synthesize keyboardAppearance = _keyboardAppearance;
@synthesize keyboardType = _keyboardType;
@synthesize spellCheckingType = _spellCheckingType;

- (instancetype)init {
  self = [super init];
  if (self) {
    _autocapitalizationType = UITextAutocapitalizationTypeNone;
    _autocorrectionType = UITextAutocorrectionTypeNo;
    _autocorrectionType = UITextAutocorrectionTypeNo;
    _keyboardType = UIKeyboardTypeASCIICapable;
    _spellCheckingType = UITextSpellCheckingTypeNo;
  }
  return self;
}

#pragma mark - UIKeyInput

- (void)insertText:(NSString*)text {
  NSLog(@"insertText: %@", text);
}

- (void)deleteBackward {
  NSLog(@"deleteBackward");
}

- (BOOL)hasText {
  return NO;  // not sure if this enables the back button.
}

#pragma mark - UIResponder

- (BOOL)canBecomeFirstResponder {
  return YES;
}

- (UIView*)inputAccessoryView {
  return nil;
}

#pragma mark - UITextInputTraits

@end
