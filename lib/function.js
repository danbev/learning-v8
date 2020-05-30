const fn = new Function("nr", "return nr;");
const nr = fn(2);
console.log(nr);
