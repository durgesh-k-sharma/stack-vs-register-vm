fn main() -> int {
    let flags: int[] = array(8193);
    let iteration: int = 0;
    let i: int = 0;
    let p: int = 0;
    let multiple: int = 0;
    let count: int = 0;
    while (iteration < 100) {
        i = 2;
        while (i <= 8192) {
            flags[i] = 1;
            i = i + 1;
        }
        p = 2;
        while (p * p <= 8192) {
            if (flags[p] != 0) {
                multiple = p * p;
                while (multiple <= 8192) {
                    flags[multiple] = 0;
                    multiple = multiple + p;
                }
            }
            p = p + 1;
        }
        count = 0;
        i = 2;
        while (i <= 8192) {
            if (flags[i] != 0) {
                count = count + 1;
            }
            i = i + 1;
        }
        iteration = iteration + 1;
    }
    return count;
}
