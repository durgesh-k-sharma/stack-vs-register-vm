fn sift_down(values: int[], start: int, end: int) -> int {
    let root: int = start;
    let child: int = 0;
    let swap: int = 0;
    while (root * 2 + 1 <= end) {
        child = root * 2 + 1;
        swap = root;
        if (values[swap] < values[child]) {
            swap = child;
        }
        if (child + 1 <= end) {
            if (values[swap] < values[child + 1]) {
                swap = child + 1;
            }
        }
        if (swap == root) {
            return 0;
        }
        let temporary: int = values[root];
        values[root] = values[swap];
        values[swap] = temporary;
        root = swap;
    }
    return 0;
}

fn main() -> int {
    let n: int = 50000;
    let values: int[] = array(50000);
    let i: int = 0;
    let start: int = 0;
    let end: int = 0;
    let temporary: int = 0;
    let checksum: int = 0;
    while (i < n) {
        values[i] = (i * 73 + 19) % 100003;
        i = i + 1;
    }
    start = n / 2 - 1;
    while (start >= 0) {
        sift_down(values, start, n - 1);
        start = start - 1;
    }
    end = n - 1;
    while (end > 0) {
        temporary = values[end];
        values[end] = values[0];
        values[0] = temporary;
        end = end - 1;
        sift_down(values, 0, end);
    }
    i = 0;
    while (i < n) {
        checksum = checksum + values[i];
        i = i + 1;
    }
    return checksum;
}
