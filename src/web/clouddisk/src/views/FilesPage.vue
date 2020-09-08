

<template>
  <el-tabs v-model="activeName" @tab-click="refresh">
    <el-tab-pane label="upload" name="first">
      <Upload />
    </el-tab-pane>
    <el-tab-pane label="myfiles" name="second">
      <MyFiles v-bind:tableData="tableData" />
    </el-tab-pane>
    <el-tab-pane label="sharedfiles" name="third">
      <SharedFiles />
    </el-tab-pane>
  </el-tabs>
</template>



<script>
import Upload from "@/components/Upload.vue";
import MyFiles from "@/components/MyFiles.vue";
import SharedFiles from "@/components/SharedFiles.vue";

export default {
  name: "FilesPage",
  components: {
    Upload,
    MyFiles,
    SharedFiles,
  },
  data() {
    return {
      activeName: "first",
      tableData: [],
    };
  },
  methods: {
    handleClick(tab, event) {
      console.log(tab, event);
    },
    refresh(tab) {
      if (tab.label == "myfiles") {
        this.axios({
          method: "post",
          url: "/files",
          data: {
            user: window.sessionStorage["user"],
            token: window.sessionStorage["token"],
          },
        }).then((res) => {
          this.tableData = res.data.files;
          // console.log("cc", res.data.files[0].user);
          // console.log(this.tableData);
        });
      }
    },
  },
};
</script>