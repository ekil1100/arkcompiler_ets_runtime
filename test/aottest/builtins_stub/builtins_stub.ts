/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
declare function print(arg:any):string;

// test new builtin constructor
function foo(a:boolean | number):void {
    let b = new Boolean(a);
    print(b);
}

foo(true);  // stub path
foo(123);   // stub path
foo(0);     // stub path

// test call builtin constructor
print(Boolean(true));   // c++ path
print(Boolean(false));  // c++ path

// test supercall
class MyBoolean extends Boolean {
    constructor(arg:boolean) {
        super(arg);             // stub path
    }
}
let b = new MyBoolean(true);
print(b instanceof MyBoolean);
print(b instanceof Boolean);