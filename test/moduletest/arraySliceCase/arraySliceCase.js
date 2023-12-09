/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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
 * @tc.name:arraySlice
 * @tc.desc:test array.slice
 * @tc.type: FUNC
 * @tc.require:
 */

const animals = ['ant', 'bison', 'camel', 'duck', 'elephant'];

print(animals.slice(2));
// Expected output: Array ["camel", "duck", "elephant"]

print(animals.slice(2, 4));
// Expected output: Array ["camel", "duck"]

print(animals.slice(1, 5));
// Expected output: Array ["bison", "camel", "duck", "elephant"]

print(animals.slice(-2));
// Expected output: Array ["duck", "elephant"]

print(animals.slice(2, -1));
// Expected output: Array ["camel", "duck"]

print(animals.slice());
// Expected output: Array ["ant", "bison", "camel", "duck", "elephant"]

print([1, 2, , 4, 5].slice(1, 4)); // [2, empty, 4]
const arrayLike = {
	  length: 3,
	  0: 2,
	  1: 3,
	  2: 4,
};
print(Array.prototype.slice.call(arrayLike, 1, 3));

const slice = Function.prototype.call.bind(Array.prototype.slice);

function list() {
	  return slice(arguments);
}

const list1 = list(1, 2, 3); // [1, 2, 3]

print(list1);

var items = ["réservé", "premier", "cliché", "communiqué", "café", "adieu"];
items.sort(function (a, b) {
  return a.localeCompare(b);
});
print(items);

const numbers1 = [3, 1, 4, 1, 5];
const sorted1 = numbers1.sort((a, b) => a - b);
sorted1[0] = 10;
print(numbers1[0]); // 10

const numbers = [3, 1, 4, 1, 5];
const sorted = [...numbers].sort((a, b) => a - b);
sorted[0] = 10;
print(numbers[0]); // 3

const arr1 = [3, 1, 4, 1, 5, 9];
const compareFn = (a, b) => (a > b ? 1 : 0);
arr1.sort(compareFn);
print(arr1);

const arr = [3, 1, 4, 1, 5, 9];
const compareFn1 = (a, b) => (a > b ? -1 : 0);
arr.sort(compareFn1);
print(arr); 

print(["a", "c", , "b"].sort()); // ['a', 'b', 'c', empty]
print([, undefined, "a", "b"].sort()); // ["a", "b", undefined, empty]
