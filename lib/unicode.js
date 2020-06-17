console.log('\u0061\u030A');

const str1 = '\u0061\u{030A}';
const str2 = 'å';

console.log(str1 === str2);
console.log(str1.normalize() === str2);

const code_point = str1.codePointAt(0);
console.log(str1.codePointAt(0), str1.codePointAt(1));
