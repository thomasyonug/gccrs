mod mem {
    extern "rust-intrinsic" {
        fn size_of<T>() -> usize;
        fn transmute<U, V>(_: U) -> V;
    }
}

fn main() {
    let a = 123;
    let _b: [u8; mem::size_of::<i32>()] = mem::transmute(a);
}
