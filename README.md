# db_final_hw

## 代码

已将环境与第五名的代码封装好，可以直接从docker上拉取：

```shell
docker pull lhygood11229/db_final_hw:1.0.0
```

然后，可以创建一个名为`hw`的容器，运行：

```shell
docker run -it --name hw lhygood11229/db_final_hw:1.0.0
```

## 测试

所有代码均已放在`/home`文件夹下，其中有两个子文件夹：`/home/submission`文件夹用于跑workload进行测试（目前只有`workload/small`，剩下的workload在老师给的网盘上，有12GB......我尝试下载但是失败了）；`/home/flomige/`则存放了排行榜第五的代码。

**首先，请用该repo替换`/home/flomige`目录。**
并编译代码（每次修改代码后，也需要再重新编译一遍）：
```shell
cd /home/flomige
./compile.sh

```

之后，在`/home/submission`文件夹下运行`./runTestharness.sh`即可。其默认跑`/home/submission/workload/small`并输出总耗时（单位ms）：

```shell
cd /home/submission
./runTestharness.sh
execute small ...
59
```

## 代码修改思路

目前的想法是主要去修改`/home/flomige/src/query_processor/QueryOpitimizer.h`文件的基数估计部分。其中，具体可以修改`estimateJoinSelectivity`与`estimateFilterCardinality`这两个函数，前者是估计join操作（例如，`stu.id = ta.id`）的基数，后者则是估计filter操作（例如，`ta.salary > 3000`）的基数。

以较为简单的`estimateFilterCardinality`为例，源代码的做法是在preprocess阶段得到每一列的min和max的value，以及非空元素的个数，然后简单地认为其符合均匀分布。于是，对于filter是`=`的情况，其直接将非空元素的个数除以`max - min`作为最终的基数估计。而对于filter是`<`或者`>`的情况，其将非空元素的个数乘以filter后的区间所占总的`max - min` 的区间的比例作为最终的基数估计。

此处，使用任何一种之前论文中的提到的基数估计的方法，应该都能比其效果更好。

`estimateJoinSelectivity`还没有看，但应该同样可以去做改动。

```c++
...
// returns the estimated filter cardinality according to the min value, max value, not null count and cardinality of the column
static uint64_t estimateFilterCardinality(
    uint64_t min,
    uint64_t max,
    uint64_t cardinality, // notNullCount
    FilterMetaData::Comparison filterPredicate,
    uint64_t constant
){
    // if the constant is not in the active domain, then this filter will eliminate all values
    if (constant < min || constant > max) {
        return 0;
    }
    // calculate the cardinality of the active domain
    double activeDomain = static_cast<double>(max - min + 1);
    // estimated cardinality for one value
    double oneValueCount = static_cast<double>(cardinality) / activeDomain;
    // return the estimated cardinality after the filter predicate
    switch (filterPredicate) {
        case FilterMetaData::Comparison::Less : {
            // number of values < constant, i.e., [min, constant): constant - min
            return static_cast<double>(constant - min)  * oneValueCount;
        };
        case FilterMetaData::Comparison::Equal : {
            return oneValueCount;
        };
        case FilterMetaData::Comparison::Greater : {
            // number of values > constant, i.e., (constant, max]: max - constant
            return static_cast<double>(max - constant) * oneValueCount;
        };
        default : throw std::runtime_error("This filter predicate is not supported.");
    }
}
...
```

