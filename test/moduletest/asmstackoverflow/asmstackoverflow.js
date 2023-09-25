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

/*
 * @tc.name:asmstackoverflow
 * @tc.desc:test stack overflow in asm
 * @tc.type: FUNC
 * @tc.require: issueI5NO8G
 */
function foo(x,y,z,a,b)
{
    foo(x,y,z,a,b)
}
try {
    foo(1, 2, 3, 4, 5)
} catch (e) {
    if ((e instanceof RangeError)) {
        print("stack overflow2!");
    }
}

const obj = {};
const pro = new Proxy({}, obj);
obj.__proto__ = pro;
try {
    obj[10];
} catch (e) {
    if (e instanceof RangeError) {
        print("proxy stackoverflow!");
    }
}