struct Foo<T> {
    a: T,
    b: bool,
}

fn test<T>(a: T) -> Foo<T> {
    Foo::<T> { a: a, b: true }
}

fn main() {
    let a;
    a = test(123);
    let aa: i32 = a.a;

    let b;
    b = test::<u32>(456);
    let bb: u32 = 456;
}
