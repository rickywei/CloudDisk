<template>
  <el-table :data="tableData" style="width: 100%">
    <el-table-column label="time" width="180">
      <template slot-scope="scope">
        <i class="el-icon-time"></i>
        <span style="margin-left: 10px">{{ scope.row.time }}</span>
      </template>
    </el-table-column>
    <el-table-column label="File" width="180">
      <template slot-scope="scope">
        <el-popover trigger="hover" placement="top">
          <p>file: {{ scope.row.filename }}</p>
          <div slot="reference" class="name-wrapper">
            <el-tag size="medium">{{ scope.row.filename }}</el-tag>
          </div>
        </el-popover>
      </template>
    </el-table-column>
    <el-table-column label="Operation">
      <template slot-scope="scope">
        <el-button size="mini" @click="download(scope.$index, scope.row)">
          <i class="el-icon-download"></i>
        </el-button>
        <el-button size="mini" @click="share(scope.$index, scope.row)">
          <i class="el-icon-share"></i>
        </el-button>
        <el-button size="mini" type="danger" @click="deleterow(scope.$index, tableData)">
          <i class="el-icon-delete"></i>
        </el-button>
      </template>
    </el-table-column>
  </el-table>
</template>

<script>
// get
// myfiles?cmd=normal

// post
// {
//         "token": "9e894efc0b2a898a82765d0a7f2c94cb",
//         user:xxxx
//     }

// response
// {
//         "user": "yoyo",
//         "md5": "e8ea6031b779ac26c319ddf949ad9d8d",
//         "time": "2017-02-26 21:35:25",
//         "filename": "test.mp4",
//         "share_status": 0,
//         "pv": 0,
//         "url":
//         "http://192.168.31.109:80/group1/M00/00/00/wKgfbViy2Z2AJ-FTAaM3As-g3Z0782.mp4",
//         "size": 27473666,
//          "type": "mp4"
//         }

export default {
  name: "MyFiles",
  props: ["tableData"],
  data() {
    return {};
  },
  methods: {
    download(index, row) {
      console.log("index", index);
      console.log("row", row);
      window.open(row.url);
    },
    share(index, row) {
      console.log(index, row);
      // console.log("url ", row.url);
      alert(row.url);
    },
    deleterow(index, row) {
      console.log(index, row);
      this.axios({
        method: "post",
        url: "/delete",
        data: {
          user: window.sessionStorage["user"],
          token: window.sessionStorage["token"],
          md5: row.md5,
          filename: row.filename,
        },
      }).then((res) => {
        if (res.status == 200) {
          if (res.data.code == "013") {
            row.splice(index, 1);
          } else {
            alert("delete failed, please check your account and passewd");
          }
        } else {
          alert("delete failed");
        }
      });
    },
  },
};
</script>