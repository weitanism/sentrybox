<script>
  export let data;

  $: sentryRecords = [...data.records].reverse();

  function formatDate(date) {
    date = new Date(date);

    const ymd =
      date.getFullYear().toString().slice(2) +
      '/' +
      (date.getMonth() + 1).toString().padStart(2, '0') +
      '/' +
      date.getDate().toString().padStart(2, '0');
    const hms =
      date.getHours().toString().padStart(2, '0') +
      ':' +
      date.getMinutes().toString().padStart(2, '0') +
      ':' +
      date.getSeconds().toString().padStart(2, '0');
    return ymd + ' ' + hms;
  }
</script>

<div class="container">
  <h1>Sentry Records</h1>

  <div class="record-list">
    {#each sentryRecords as record}
      <a href="/sentry/{record.directory}">view</a>
      <span class="date">{formatDate(record.event.timestamp)}</span>
      <span class="reason">{record.event.reason}</span>
      <span class="city">{record.event.city}</span>
    {/each}
  </div>
</div>

<style>
  .container {
    display: flex;
    flex-direction: column;
    font-family: monospace;
    padding: 0 4px;
  }

  .record-list {
    display: grid;
    grid-template-columns: 1fr 2fr 3fr 3fr;
    justify-items: center;
  }
</style>
