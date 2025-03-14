// { dg-output "rust/execute/torture/builtin_macros1.rs" }
macro_rules! file {
    () => {{}};
}

extern "C" {
    fn printf(fmt: *const i8, ...);
}

fn print(s: &str) {
    printf("%s\n\0" as *const str as *const i8, s);
}

fn main() -> i32 {
    print(file!());

    0
}
