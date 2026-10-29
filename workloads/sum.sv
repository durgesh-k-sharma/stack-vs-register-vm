fn main() -> int {
    let total: int = 0;
    let i: int = 1;
    while (i <= 20000000) {
        total = total + i;
        i = i + 1;
    }
    return total;
}
