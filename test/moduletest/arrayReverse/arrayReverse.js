const array1 = ['one', 'two', 'three'];
print('array1:', array1);
// Expected output: "array1:" Array ["one", "two", "three"]

const reversed1 = array1.reverse();
print('reversed1:', reversed1);
// Expected output: "reversed1:" Array ["three", "two", "one"]

// Careful: reverse is destructive -- it changes the original array.
print('array1:', array1);
// Expected output: "array1:" Array ["three", "two", "one"]

print([1, , 3].reverse()); // [3, empty, 1]
print([1, , 3, 4].reverse()); // [4, 3, empty, 1]

const numbers = [3, 2, 4, 1, 5];
// [...numbers] 创建一个浅拷贝，因此 reverse() 不会改变原始数据
const reverted = [...numbers].reverse();
reverted[0] = 5;
print(numbers[0]); // 3

const numbers1 = [3, 2, 4, 1, 5];
const reversed = numbers1.reverse();
// numbers1 和 reversed 的顺序都是颠倒的 [5, 1, 4, 2, 3]
reversed[0] = 5;
print(numbers1[0]); // 5
