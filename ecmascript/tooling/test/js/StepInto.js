/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

function countToTen() {
    var a = 1;
    a = 2;
    a = 3;
    a = 4;
    a = 5;
    a = 6;
    a = 7;
    a = 8;
    a = 9;
    next();
    a = 10;
}

function next() {
    print("B->next");
    var a = 1;
    a = 2;
    next_c();
    a = 3;
}

function next_c() {
    print("C->next");
    var a = 1;
    a = 2;
    a = 3;
}

print("A->countToTen");
countToTen();
var b = 10;
b = 11;
print("D->end");
