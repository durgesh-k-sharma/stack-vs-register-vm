fn main() -> int {
    let n: int = 50;
    let a: int[] = array(2500);
    let b: int[] = array(2500);
    let c: int[] = array(2500);
    let i: int = 0;
    let j: int = 0;
    let k: int = 0;
    let index: int = 0;
    let total: int = 0;
    let checksum: int = 0;
    while (i < 2500) {
        a[i] = (i * 3 + 1) % 97;
        b[i] = (i * 7 + 2) % 89;
        i = i + 1;
    }
    i = 0;
    while (i < n) {
        j = 0;
        while (j < n) {
            k = 0;
            total = 0;
            while (k < n) {
                total = total + a[i * n + k] * b[k * n + j];
                k = k + 1;
            }
            index = i * n + j;
            c[index] = total;
            j = j + 1;
        }
        i = i + 1;
    }
    i = 0;
    while (i < 2500) {
        checksum = checksum + c[i];
        i = i + 1;
    }
    return checksum;
}
