const obj = { name: 'Fletch' };

const descriptor = Object.getOwnPropertyDescriptor(obj, 'name');
console.log(descriptor);
